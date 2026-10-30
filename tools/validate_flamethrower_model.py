"""Fresh-import and raw glTF checks, plus reproducible runtime handoff manifest.

Blender --factory-startup --background --python tools/validate_flamethrower_model.py
Run validate_flamethrower_loader.cpp separately to confirm the actual game loader.
"""
from pathlib import Path
import hashlib
import json
import math
import re
import struct

import bpy
import bmesh
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT/'assets/models/buildings/flamethrower.glb'
MANIFEST = ROOT/'art/models/flamethrower_manifest.json'
data = PATH.read_bytes()
assert struct.unpack_from('<III',data) == (0x46546c67,2,len(data))
size,kind = struct.unpack_from('<II',data,12)
assert kind == 0x4e4f534a
doc = json.loads(data[20:20+size])
binary_size,kind = struct.unpack_from('<II',data,20+size)
assert kind == 0x004e4942
binary = data[28+size:28+size+binary_size]
assert not doc.get('extensionsRequired')
assert 'animations' not in doc and 'skins' not in doc
assert 'cameras' not in doc and len(doc['scenes']) == 1
assert all('uri' not in b for b in doc['buffers'])
assert len(doc['images']) == 1 and 'uri' not in doc['images'][0]
view = doc['bufferViews'][doc['images'][0]['bufferView']]
offset = view.get('byteOffset',0)
png = binary[offset:offset+view['byteLength']]
assert png == (ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()
assert png == (ROOT/'art/references/kenney_tower_defense/Textures/colormap.png').read_bytes()

def accessor(index):
    a = doc['accessors'][index]
    v = doc['bufferViews'][a['bufferView']]
    fmt = {5126:'f',5125:'I',5123:'H',5121:'B'}[a['componentType']]
    width = {'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
    row = '<'+fmt*width
    start = v.get('byteOffset',0)+a.get('byteOffset',0)
    stride = v.get('byteStride',struct.calcsize(row))
    return [struct.unpack_from(row,binary,start+i*stride) for i in range(a['count'])]

def transform(node):
    if 'matrix' in node:
        return Matrix([node['matrix'][i::4] for i in range(4)])
    x,y,z,w = node.get('rotation',[0,0,0,1])
    return Matrix.LocRotScale(Vector(node.get('translation',[0,0,0])),
                             Quaternion((w,x,y,z)),Vector(node.get('scale',[1,1,1])))

nodes = doc['nodes']
names = [n['name'] for n in nodes]
assert len(set(names)) == len(names)
assert all(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*',n) for n in names), names
by_name = {n['name']:i for i,n in enumerate(nodes)}
expected = {'asset_root','base_mesh','yaw_mesh','pitch_mesh','yaw_pivot','pitch_pivot',
            'MUZZLE_01','socket_base','socket_ui_top','socket_effect_fuel','COL_BOX_BODY'}
assert set(names) == expected
assert doc['scenes'][0]['nodes'] == [by_name['asset_root']]
parents = {}
world = {}
def visit(i,parent=None):
    parents[i] = parent
    world[i] = (world[parent] if parent is not None else Matrix.Identity(4)) @ transform(nodes[i])
    for child in nodes[i].get('children',[]):visit(child,i)
visit(by_name['asset_root'])
assert all(max(abs(v) for row in (transform(n)-Matrix.Translation(Vector(n.get('translation',[0,0,0])))).to_3x3() for v in row)<1e-5
           for n in nodes), 'unexpected rotation or scale'
assert transform(nodes[by_name['asset_root']]) == Matrix.Identity(4)
assert parents[by_name['MUZZLE_01']] == by_name['pitch_pivot']
assert parents[by_name['COL_BOX_BODY']] == by_name['asset_root']
assert parents[by_name['socket_ui_top']] == by_name['asset_root']

def close_vec(actual,expected):
    assert (Vector(actual)-Vector(expected)).length < 1e-5,(tuple(actual),expected)
close_vec(world[by_name['MUZZLE_01']].translation,(0,.924,-.59))
close_vec(world[by_name['MUZZLE_01']].to_3x3() @ Vector((0,0,-1)),(0,0,-1))
close_vec(world[by_name['MUZZLE_01']].to_3x3() @ Vector((0,1,0)),(0,1,0))
close_vec(world[by_name['yaw_pivot']].translation,(0,.624,0))
close_vec(world[by_name['pitch_pivot']].translation,(0,.924,0))
close_vec(world[by_name['socket_ui_top']].translation,(0,1.24,0))

visible_points=[]; collider_points=[]; counts={}; primitive_indices={}
render_index=0
materials=set()
for ni,n in enumerate(nodes):
    if 'mesh' not in n:continue
    primitives=doc['meshes'][n['mesh']]['primitives']
    assert len(primitives)==1
    p=primitives[0]
    assert p.get('mode',4)==4
    attrs=p['attributes']
    positions=accessor(attrs['POSITION'])
    points=[world[ni] @ Vector(v) for v in positions]
    indices=[v[0] for v in accessor(p['indices'])]
    normals=accessor(attrs['NORMAL'])
    assert all(all(math.isfinite(c) for c in v) and abs(Vector(v).length-1)<1e-4 for v in normals)
    assert len(indices)%3==0
    # No degenerate/duplicated geometric triangles, including reverse duplicates.
    face_keys=set()
    for i in range(0,len(indices),3):
        a,b,c=[Vector(positions[j]) for j in indices[i:i+3]]
        assert (b-a).cross(c-a).length > 1e-9
        key=tuple(sorted(tuple(round(v,6) for v in co) for co in (a,b,c)))
        assert key not in face_keys,'duplicate face '+n['name']
        face_keys.add(key)
    counts[n['name']]=len(indices)//3
    primitive_indices[n['name']]=render_index;render_index+=1
    if n['name'].startswith('COL_'):
        collider_points.extend(points)
        assert len(set(positions))==8 and len(indices)==36
    else:
        visible_points.extend(points);materials.add(p['material'])
        assert 'TEXCOORD_0' in attrs
        for u,v in accessor(attrs['TEXCOORD_0']):
            # glTF UV has its vertical axis flipped relative to Blender.
            bv=1-v
            assert min(abs(u-col) for col in (.09375,.34375,.84375,.59375,.46875,.96875))<1e-5
            assert .025-1e-5<=bv<=.225+1e-5 or .275-1e-5<=bv<=.475+1e-5
assert render_index==4 and len(materials)==1
assert len(doc['materials'])==2 # one visible palette, one diagnostic collider
mat=doc['materials'][next(iter(materials))]['pbrMetallicRoughness']
assert mat.get('metallicFactor',1)==0 and mat.get('roughnessFactor',1)==1
assert 'baseColorTexture' in mat
def bounds(points):
    lo=[min(p[i] for p in points) for i in range(3)]
    hi=[max(p[i] for p in points) for i in range(3)]
    return {'min':lo,'max':hi,'size':[hi[i]-lo[i] for i in range(3)]}
visible_bounds=bounds(visible_points);collision_bounds=bounds(collider_points)
assert abs(visible_bounds['min'][1])<1e-5
assert abs(visible_bounds['size'][0]-1.2)<1e-5
assert abs(visible_bounds['size'][1]-1.14)<1e-5
close_vec(collision_bounds['min'],(-.75,0,-.75))
close_vec(collision_bounds['max'],(.75,1.14,.75))

# Verify the editable file independently: final metre dimensions, neutral
# object transforms, packed original PNG, and separate studio/collision data.
with bpy.data.libraries.load(str(ROOT/'art/models/flamethrower.blend'),link=False) as (source,target):
    assert source.scenes==['Flamethrower']
    target.scenes=['Flamethrower']
editable=target.scenes[0]
startup_scene=bpy.context.scene
bpy.context.window.scene=editable
bpy.context.view_layer.update()
assert {c.name for c in editable.collection.children}=={'FLAMETHROWER_MODEL','FLAMETHROWER_COLLISION','FLAMETHROWER_STUDIO'}
assert all(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*',o.name) for o in editable.objects)
assert editable.unit_settings.scale_length==1
for o in editable.objects:
    if o.type not in ('MESH','EMPTY'):continue
    close_vec(o.scale,(1,1,1));close_vec(o.rotation_euler,(0,0,0))
assert len([o for o in editable.objects if o.type=='MESH'])==4
assert editable.objects['COL_BOX_BODY'].hide_render
close_vec(editable.objects['MUZZLE_01'].matrix_world.translation,(0,.59,.924))
image=next(n.image for n in editable.objects['base_mesh'].data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE')
assert image.packed_file and bytes(image.packed_file.data)==png
def part_tree(obj,group_name=None):
    obj.data.calc_loop_triangles()
    triangles=[tuple(t.vertices) for t in obj.data.loop_triangles]
    if group_name:
        group=obj.vertex_groups[group_name].index
        vertices={v.index for v in obj.data.vertices if any(g.group==group for g in v.groups)}
        triangles=[t for t in triangles if all(i in vertices for i in t)]
    return BVHTree.FromPolygons([obj.matrix_world@v.co for v in obj.data.vertices],triangles,all_triangles=True)
source_yaw=editable.objects['yaw_pivot'];source_yaw_mesh=editable.objects['yaw_mesh']
source_base_tree=part_tree(editable.objects['base_mesh'])
rear_parts=['flamethrower_fuel_tank','flamethrower_tank_cap','flamethrower_tank_boot',
            'flamethrower_tank_band','flamethrower_tank_bracket','flamethrower_fuel_hose']
yaw_base_overlaps=[]
for angle in range(360):
    source_yaw.rotation_euler.z=math.radians(angle)
    bpy.context.view_layer.update()
    for part in rear_parts:
        if part_tree(source_yaw_mesh,part).overlap(source_base_tree):yaw_base_overlaps.append([angle,part])
source_yaw.rotation_euler.z=0;bpy.context.view_layer.update()
print('YAW_BASE_OVERLAPS',yaw_base_overlaps)
assert not yaw_base_overlaps,'tank/hose intersects fixed base during yaw'
hose_tree=part_tree(source_yaw_mesh,'flamethrower_fuel_hose')
for part in ['flamethrower_yoke_-1','flamethrower_yoke_1','flamethrower_turntable',
             'flamethrower_yoke_foot','flamethrower_tank_boot','flamethrower_tank_band',
             'flamethrower_tank_cap','flamethrower_tank_bracket']:
    assert not hose_tree.overlap(part_tree(source_yaw_mesh,part)),'hose intersects '+part
# Fuel-wall and axle-coupling contact are the two intentional hose endpoints.
bpy.context.window.scene=startup_scene
for o in list(editable.objects):bpy.data.objects.remove(o,do_unlink=True)
bpy.data.scenes.remove(editable)

# Import into an isolated clean scene, without consulting the source blend.
scene=bpy.data.scenes.new('Flamethrower_validation')
bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(PATH))
objects={o.name:o for o in scene.objects}
assert set(objects)==expected
for name in ('base_mesh','yaw_mesh','pitch_mesh','COL_BOX_BODY'):
    o=objects[name]
    bm=bmesh.new();bm.from_mesh(o.data)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    assert all(e.is_manifold for e in bm.edges),'non-manifold '+name
    assert bm.calc_volume(signed=True)>0,'inverted '+name
    bm.free()
    if not name.startswith('COL_'):assert o.data.uv_layers.active
close_vec(objects['MUZZLE_01'].matrix_world.translation,(0,.59,.924))

def tree(obj):
    obj.data.calc_loop_triangles()
    return BVHTree.FromPolygons([obj.matrix_world @ v.co for v in obj.data.vertices],
        [tuple(t.vertices) for t in obj.data.loop_triangles],all_triangles=True,epsilon=0)

# Exhaustive one-degree elevation sampling at cardinal yaw angles. Surface
# overlap checks are complemented by minimum-height and footprint checks.
yaw=objects['yaw_pivot'];pitch=objects['pitch_pivot']
yaw.rotation_mode='XYZ';pitch.rotation_mode='XYZ'
base_matrix=objects['base_mesh'].matrix_world.copy()
collision_matrix=objects['COL_BOX_BODY'].matrix_world.copy()
base_tree=tree(objects['base_mesh'])
overlaps=[]
pose_min_height=math.inf;pose_radius=0
for degrees_yaw in (0,90,180,270):
    yaw.rotation_euler.z=math.radians(degrees_yaw)
    for degrees_pitch in range(-10,31):
        pitch.rotation_euler.x=math.radians(degrees_pitch)
        bpy.context.view_layer.update()
        moving_tree=tree(objects['pitch_mesh'])
        if moving_tree.overlap(tree(objects['yaw_mesh'])) or moving_tree.overlap(base_tree):
            overlaps.append([degrees_yaw,degrees_pitch])
        for o in (objects['yaw_mesh'],objects['pitch_mesh']):
            for v in o.data.vertices:
                p=o.matrix_world @ v.co
                pose_min_height=min(pose_min_height,p.z)
                pose_radius=max(pose_radius,math.hypot(p.x,p.y))
        # Muzzle follows the barrel's entire parent chain.
        f=objects['MUZZLE_01'].matrix_world.to_3x3() @ Vector((0,1,0))
        assert abs(f.z-math.sin(math.radians(degrees_pitch)))<1e-5
        assert objects['base_mesh'].matrix_world==base_matrix
        assert objects['COL_BOX_BODY'].matrix_world==collision_matrix
yaw.rotation_euler.z=0;pitch.rotation_euler.x=0;bpy.context.view_layer.update()
print('POSE_OVERLAPS',overlaps)
assert not overlaps,'pitch intersects fixed geometry'
assert pose_min_height>=0 and pose_radius<.75,'ground or full-yaw collider/adjacent footprint conflict'

def rounded(v):return [round(float(c),6) for c in v]
transforms={}
for name in ('asset_root','yaw_pivot','pitch_pivot','MUZZLE_01','socket_base','socket_ui_top','socket_effect_fuel','COL_BOX_BODY'):
    i=by_name[name];n=nodes[i]
    transforms[name]={'parent':nodes[parents[i]]['name'] if parents[i] is not None else None,
                     'local_translation':rounded(n.get('translation',[0,0,0])),
                     'local_rotation_xyzw':rounded(n.get('rotation',[0,0,0,1])),
                     'local_scale':rounded(n.get('scale',[1,1,1])),
                     'position_relative_to_asset_root':rounded(world[i].translation)}
manifest={
    'asset':'flamethrower','contract':'docs/DEFENSE_MODEL_HANDOFF_RU.md',
    'files':{'blend':'art/models/flamethrower.blend','runtime_glb':str(PATH.relative_to(ROOT)),
             'reference_glb':'art/models/weapon-flamethrower.glb','reference_length_multiplier':.5,
             'generator':'tools/create_flamethrower_model.py','validator':'tools/validate_flamethrower_model.py',
             'loader_probe':'tools/validate_flamethrower_loader.cpp',
             'handoff':'art/models/flamethrower_handoff.md',
             'previews':['art/models/flamethrower_views/'+n+'.png' for n in ('beauty','front','side','back','top','with_cannon')],
             'comparison':'art/models/flamethrower_comparison.png'},
    'units':'metres','coordinates':'GLB: +Y up, -Z forward, +X right; Blender: +Z up, +Y forward',
    'visible_bounds_glb':{k:rounded(v) for k,v in visible_bounds.items()},
    'colliders':[{'name':'COL_BOX_BODY','type':'box','parent':'asset_root','walkable':False,
                  'bounds_glb':{k:rounded(v) for k,v in collision_bounds.items()},
                  'vertices_unique':8,'triangles':12,'role':'Fixed full-yaw horizontal envelope; visible base is smaller.'}],
    'footprint':{'recommended_cells':[2,2],'x_range':[-1,1],'z_range':[-1,1],
                 'status':'Must be assigned to the new building type in game code; GLB does not set footprint or movement blocking.'},
    'counts':{'visible_meshes':3,'visible_primitives':3,'visible_materials':1,
              'collision_meshes':1,'collision_primitives':1,'collision_materials':1,
              'total_meshes':4,'total_primitives':4,'total_materials':2,
              'visible_triangles':sum(v for k,v in counts.items() if not k.startswith('COL_')),
              'total_triangles':sum(counts.values()),'triangles_by_mesh':counts},
    'render_indices_after_collision_removal':{'base_mesh':0,'pitch_mesh':1,'yaw_mesh':2},
    'export_primitive_indices_in_node_order':primitive_indices,
    'transforms_glb':transforms,
    'muzzles':[{'name':'MUZZLE_01','forward_relative_to_root':[0,0,-1],'up_relative_to_root':[0,1,0],
                'offset_ahead_of_lip_m':.02}],
    'motion':{'yaw_axis_glb':[0,1,0],'yaw_degrees':[0,360],
              'pitch_axis_glb':[1,0,0],'pitch_elevation_degrees':[-10,30],
              'positive_pitch':'raises muzzle','recoil':{'present':False,'travel_m':0},
              'groups':{'base_mesh':'Fixed foot, pedestal, braces and base bolts.',
                        'yaw_mesh':'Turntable, wooden clevis, bearings, upright fuel reservoir, mounting bracket and hose to the pitch axle.',
                        'pitch_mesh':'Receiver, orange barrel shroud, hollow nozzle and pilot nozzle.'},
              'pose_validation':{'yaw_samples':[0,90,180,270],'pitch_step_degrees':1,
                                 'poses_checked':164,'surface_intersections':0,
                                 'minimum_moving_height_m':round(pose_min_height,6),
                                 'maximum_horizontal_radius_m':round(pose_radius,6)}},
    'palette':{'path':'assets/models/buildings/Textures/colormap.png','dimensions':[512,512],
               'sha256':hashlib.sha256(png).hexdigest(),'embedded_glb':True,'packed_blend':True,
               'license':'Kenney Tower Defense Kit 2.1, CC0',
               'license_file':'art/references/kenney_tower_defense/LICENSE.txt'},
    'geometry_source':'Original authored flamethrower geometry matching the supplied Kenney examples; kit palette reused.',
    'levels':{'count':1,'kind':'complete base model','overlays':[]},
    'effects':'No baked flame, smoke, light or UI geometry. Fuel socket is a future effect attachment.',
    'integration':{'rig_connected':False,'playtested':False,
                    'note':'Export and fresh Blender import checked. Loader verification is a separate probe; new building, aiming and VFX still need game integration.'},
    'validation':{'raw_glb':True,'fresh_import':True,'normals_uv_manifold':True,'pose_sweep':True,
                  'embedded_palette_byte_identical':True,'editable_blend_checked':True,
                  'yaw_tank_hose_sweep_degrees':{'min':0,'max':359,'step':1,'poses':360},
                  'yaw_tank_hose_base_intersections':0,'hose_mount_intersections':0,
                  'glb_sha256':hashlib.sha256(data).hexdigest()}
}
MANIFEST.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print('FLAMETHROWER_VALIDATED',json.dumps(manifest['counts']),hashlib.sha256(png).hexdigest())

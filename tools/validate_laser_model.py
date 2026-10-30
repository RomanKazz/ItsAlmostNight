"""Fresh Blender import, raw GLB checks and laser manifest.

Blender --factory-startup --background --python-exit-code 1 --python tools/validate_laser_model.py
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

ROOT=Path(__file__).resolve().parents[1]
PATH=ROOT/'assets/models/buildings/laser.glb'
blob=PATH.read_bytes()
assert struct.unpack_from('<III',blob)==(0x46546c67,2,len(blob))
size,kind=struct.unpack_from('<II',blob,12);assert kind==0x4e4f534a
doc=json.loads(blob[20:20+size])
binary_size,kind=struct.unpack_from('<II',blob,20+size);assert kind==0x004e4942
binary=blob[28+size:28+size+binary_size]
assert not doc.get('extensionsRequired') and 'animations' not in doc and 'skins' not in doc
assert 'cameras' not in doc and len(doc['scenes'])==1
assert all('uri' not in b for b in doc['buffers'])
assert len(doc['images'])==1 and 'uri' not in doc['images'][0]
view=doc['bufferViews'][doc['images'][0]['bufferView']];off=view.get('byteOffset',0)
png=binary[off:off+view['byteLength']]
assert png==(ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()
assert png==(ROOT/'art/references/kenney_tower_defense/Textures/colormap.png').read_bytes()
def values(index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
    fmt={5126:'f',5125:'I',5123:'H',5121:'B'}[a['componentType']]
    width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];row='<'+fmt*width
    start=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',struct.calcsize(row))
    return [struct.unpack_from(row,binary,start+i*stride) for i in range(a['count'])]
def transform(n):
    if 'matrix' in n:return Matrix([n['matrix'][i::4] for i in range(4)])
    x,y,z,w=n.get('rotation',[0,0,0,1])
    return Matrix.LocRotScale(Vector(n.get('translation',[0,0,0])),Quaternion((w,x,y,z)),Vector(n.get('scale',[1,1,1])))
def close(actual,expected):assert (Vector(actual)-Vector(expected)).length<1e-5,(tuple(actual),expected)
nodes=doc['nodes'];names=[n['name'] for n in nodes]
expected={'asset_root','base_mesh','energy_mesh','COL_BOX_BODY','MUZZLE_01','socket_base','socket_ui_top','socket_effect_energy','yaw_pivot','pitch_pivot','yaw_mesh','pitch_mesh'}
assert set(names)==expected and len(names)==len(expected)
assert all(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*',name) for name in names)
by_name={n['name']:i for i,n in enumerate(nodes)}
assert doc['scenes'][0]['nodes']==[by_name['asset_root']]
parents={};world={}
def visit(i,parent=None):
    parents[i]=parent;world[i]=(world[parent] if parent is not None else Matrix.Identity(4))@transform(nodes[i])
    for j in nodes[i].get('children',[]):visit(j,i)
visit(by_name['asset_root'])
assert transform(nodes[by_name['asset_root']])==Matrix.Identity(4)
for i,n in enumerate(nodes):
    close(n.get('scale',[1,1,1]),[1,1,1])
    assert max(abs(c) for row in (transform(n)-Matrix.Translation(Vector(n.get('translation',[0,0,0])))) for c in row)<1e-5
    if n['name']!='asset_root':
        expected_parent={'yaw_mesh':'yaw_pivot','pitch_pivot':'yaw_pivot','pitch_mesh':'pitch_pivot','energy_mesh':'pitch_pivot','MUZZLE_01':'pitch_pivot','socket_effect_energy':'pitch_pivot'}.get(n['name'],'asset_root')
        assert parents[i]==by_name[expected_parent]
close(world[by_name['pitch_pivot']].translation,(0,1.020,.170))
close(nodes[by_name['MUZZLE_01']].get('translation',[0,0,0]),(0,0,-.629))
close(world[by_name['MUZZLE_01']].translation,(0,1.020,-.459))
close(world[by_name['MUZZLE_01']].to_3x3()@Vector((0,0,-1)),(0,0,-1))
close(world[by_name['MUZZLE_01']].to_3x3()@Vector((0,1,0)),(0,1,0))
close(world[by_name['socket_ui_top']].translation,(0,1.480,0))
visible=[];collider=[];counts={};primitive_order={};visible_materials=set()
render_index=0
for i,n in enumerate(nodes):
    if 'mesh' not in n:continue
    primitives=doc['meshes'][n['mesh']]['primitives'];assert len(primitives)==1
    p=primitives[0];assert p.get('mode',4)==4
    attrs=p['attributes'];positions=values(attrs['POSITION']);normals=values(attrs['NORMAL'])
    indices=[v[0] for v in values(p['indices'])];assert len(indices)%3==0
    assert all(all(math.isfinite(c) for c in v) and abs(Vector(v).length-1)<1e-4 for v in normals)
    faces=set()
    for start in range(0,len(indices),3):
        tri=[Vector(positions[j]) for j in indices[start:start+3]]
        normal=(tri[1]-tri[0]).cross(tri[2]-tri[0]);assert normal.length>1e-9
        assert all(normal.dot(Vector(normals[j]))>0 for j in indices[start:start+3]),'normal points inward'
        key=tuple(sorted(tuple(round(c,6) for c in v) for v in tri));assert key not in faces
        faces.add(key)
    counts[n['name']]=len(indices)//3;primitive_order[n['name']]=render_index;render_index+=1
    points=[world[i]@Vector(v) for v in positions]
    if n['name'].startswith('COL_'):
        collider.extend(points);assert len(set(positions))==8 and len(indices)==36
    else:
        visible.extend(points);visible_materials.add(p['material'])
        assert 'TEXCOORD_0' in attrs
        for u,v in values(attrs['TEXCOORD_0']):
            assert min(abs(u-col) for col in (.09375,.84375,.59375,.46875,.03125))<1e-5
            bv=1-v
            assert .025-1e-5<=bv<=.225+1e-5 or .275-1e-5<=bv<=.475+1e-5
assert render_index==5 and len(visible_materials)==2 and len(doc['materials'])==3
for index in visible_materials:
    mat=doc['materials'][index]['pbrMetallicRoughness']
    assert mat.get('metallicFactor',1)==0 and mat.get('roughnessFactor',1)==1 and 'baseColorTexture' in mat
def bounds(points):
    lo=[min(v[i] for v in points) for i in range(3)];hi=[max(v[i] for v in points) for i in range(3)]
    return {'min':lo,'max':hi,'size':[hi[i]-lo[i] for i in range(3)]}
vb=bounds(visible);cb=bounds(collider)
close(vb['min'],(-.6,0,-.6));close(vb['max'],(.6,1.318,.6))
close(cb['min'],(-.6,0,-.6));close(cb['max'],(.6,1.40,.6))


# Inspect editable source and exercise its complete aiming assembly.
with bpy.data.libraries.load(str(ROOT/'art/models/laser.blend'),link=False) as (source,target):
    assert source.scenes==['LASER'];target.scenes=['LASER']
editable=target.scenes[0];startup=bpy.context.scene
bpy.context.window.scene=editable;bpy.context.view_layer.update()
assert {c.name for c in editable.collection.children}=={'LASER_MODEL','LASER_COLLISION','LASER_STUDIO'}
assert editable.unit_settings.scale_length==1
assert all(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*',o.name) for o in editable.objects)
assert len([o for o in editable.objects if o.type=='MESH'])==5
assert editable.objects['COL_BOX_BODY'].hide_render
im=next(n.image for n in editable.objects['base_mesh'].data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE')
assert im.packed_file and bytes(im.packed_file.data)==png
for o in editable.objects:
    if o.type in ('MESH','EMPTY'):
        close(o.scale,(1,1,1));close(o.rotation_euler,(0,0,0))
close(editable.objects['MUZZLE_01'].matrix_world.translation,(0,.459,1.020))
def tree(obj,exclude=None):
    obj.data.calc_loop_triangles()
    faces=[tuple(t.vertices) for t in obj.data.loop_triangles]
    if exclude:
        group=obj.vertex_groups[exclude].index
        omitted={v.index for v in obj.data.vertices if any(g.group==group for g in v.groups)}
        faces=[f for f in faces if not all(v in omitted for v in f)]
    return BVHTree.FromPolygons([obj.matrix_world@v.co for v in obj.data.vertices],faces,all_triangles=True)
base=editable.objects['base_mesh'];mount=editable.objects['yaw_mesh']
head=editable.objects['pitch_mesh'];core=editable.objects['energy_mesh']
yaw=editable.objects['yaw_pivot'];pitch=editable.objects['pitch_pivot'];col=editable.objects['COL_BOX_BODY']
# User-reported defects: the platform must support all clevis-foot corners,
# and the axle must sit behind the cooling neck, inside the reactor bearings.
def group_ids(obj,name):
    group=obj.vertex_groups[name].index
    return {v.index for v in obj.data.vertices if any(g.group==group for g in v.groups)}
platform_ids=group_ids(mount,'laser_mount_foot');mount.data.calc_loop_triangles()
platform_faces=[tuple(t.vertices) for t in mount.data.loop_triangles if all(i in platform_ids for i in t.vertices)]
platform=BVHTree.FromPolygons([mount.matrix_world@v.co for v in mount.data.vertices],platform_faces,all_triangles=True)
supported_corners=0
for label in ('laser_yoke_left','laser_yoke_right'):
    points=[mount.matrix_world@mount.data.vertices[i].co for i in group_ids(mount,label)]
    bottom=min(v.z for v in points)
    for point in points:
        if abs(point.z-bottom)>1e-5:continue
        hit,normal,index,distance=platform.ray_cast(Vector((point.x,point.y,.750)),Vector((0,0,-1)),.1)
        assert hit is not None and hit.z>=bottom,'unsupported clevis foot'
        supported_corners+=1
assert supported_corners==8
axle=[head.matrix_world@head.data.vertices[i].co for i in group_ids(head,'laser_trunnion_axle')]
neck=[head.matrix_world@head.data.vertices[i].co for i in group_ids(head,'laser_neck')]
assert max(v.y for v in axle)<min(v.y for v in neck)-.1,'axle crosses exposed neck'
close(pitch.matrix_world.translation,(0,-.170,1.020))
base_matrix=base.matrix_world.copy();col_matrix=col.matrix_world.copy();base_tree=tree(base)
poses=sorted(set((a,p) for a in range(360) for p in (-12,0,20)) |
             set((a,p) for a in range(0,360,45) for p in range(-12,21)))
overlaps=[];min_z=math.inf;max_z=-math.inf;max_radius=0
for angle,elevation in poses:
    yaw.rotation_euler.z=math.radians(angle);pitch.rotation_euler.x=math.radians(elevation)
    bpy.context.view_layer.update()
    mount_tree=tree(mount)
    # The drive axle intentionally occupies the bearing; other parts must clear it.
    for obj,exclude in ((head,'laser_trunnion_axle'),(core,None)):
        moving=tree(obj,exclude)
        if moving.overlap(mount_tree) or moving.overlap(base_tree):
            overlaps.append([angle,elevation,obj.name])
    for obj in (mount,head,core):
        for v in obj.data.vertices:
            p=obj.matrix_world@v.co
            min_z=min(min_z,p.z);max_z=max(max_z,p.z);max_radius=max(max_radius,math.hypot(p.x,p.y))
    assert base.matrix_world==base_matrix and col.matrix_world==col_matrix
    f=editable.objects['MUZZLE_01'].matrix_world.to_3x3()@Vector((0,1,0))
    assert abs(f.z-math.sin(math.radians(elevation)))<1e-5
yaw.rotation_euler.z=0;pitch.rotation_euler.x=0;bpy.context.view_layer.update()
print('LASER_POSE_OVERLAPS',overlaps[:30],'total',len(overlaps))
assert not overlaps,'aiming assembly intersects fixed structure'
assert min_z>0 and max_z<1.40 and max_radius<.60,'aiming assembly exits collider/footprint'
bpy.context.window.scene=startup
for o in list(editable.objects):bpy.data.objects.remove(o,do_unlink=True)
bpy.data.scenes.remove(editable)

# Fresh GLB import verifies exported geometry independent of source objects.
scene=bpy.data.scenes.new('LASER_VALIDATION');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(PATH));bpy.context.view_layer.update()
assert {o.name for o in scene.objects}==expected
for o in scene.objects:
    if o.type!='MESH':continue
    bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    assert all(e.is_manifold for e in bm.edges),'non-manifold '+o.name
    assert bm.calc_volume(signed=True)>0,'inverted '+o.name;bm.free()
    if not o.name.startswith('COL_'):assert o.data.uv_layers.active
close(scene.objects['MUZZLE_01'].matrix_world.translation,(0,.459,1.020))
assert scene.objects['energy_mesh'].parent==scene.objects['pitch_pivot']
def rounded(v):return [round(float(c),6) for c in v]
transforms={}
for i,n in enumerate(nodes):
    if 'mesh' in n and not n['name'].startswith('COL_'):continue
    transforms[n['name']]={'parent':nodes[parents[i]]['name'] if parents[i] is not None else None,
        'local_translation':rounded(n.get('translation',[0,0,0])),
        'local_rotation_xyzw':rounded(n.get('rotation',[0,0,0,1])),
        'local_scale':rounded(n.get('scale',[1,1,1])),
        'position_relative_to_asset_root':rounded(world[i].translation)}
render_order={name:idx for idx,name in enumerate(name for name,index in sorted(primitive_order.items(),key=lambda item:item[1]) if not name.startswith('COL_'))}
manifest={
    'asset':'laser','concept':'User orange/cyan laser concept, adapted to existing Kenney defense models.',
    'files':{'blend':'art/models/laser.blend','runtime_glb':str(PATH.relative_to(ROOT)),
        'generator':'tools/create_laser_model.py','revision_script':'tools/refine_laser_support.py','validator':'tools/validate_laser_model.py',
        'loader_probe':'tools/validate_laser_loader.cpp','loader_report':'art/models/laser_views/loader_report.json','handoff':'art/models/laser_handoff.md',
        'preview':'art/models/laser_preview.png','comparison':'art/models/laser_comparison.png',
        'views':['art/models/laser_views/'+n+'.png' for n in ('beauty','front','side','back','top')]},
    'revision':{'date':'2026-10-15','turntable_radius_m':.455,'support_platform_radius_m':.435,
        'pitch_pivot_moved_back_m':.170,'note':'Full clevis support; axle moved into reactor housing; neutral muzzle position unchanged.'},
    'units':'metres','coordinates':'GLB +Y up, -Z forward, +X right; Blender +Z up, +Y forward',
    'visible_bounds_glb':{k:rounded(v) for k,v in vb.items()},
    'colliders':[{'name':'COL_BOX_BODY','type':'box','parent':'asset_root','walkable':False,
        'bounds_glb':{k:rounded(v) for k,v in cb.items()},'unique_vertices':8,'triangles':12,
        'role':'Fixed body and full aiming height envelope; independent of build/movement footprint.'}],
    'footprint':{'recommended_cells':[2,2],'x_range':[-1,1],'z_range':[-1,1],
        'note':'Must be assigned in game code; GLB does not configure build or movement blocking.'},
    'counts':{'visible_meshes':4,'visible_primitives':4,'visible_materials':2,
        'collision_meshes':1,'collision_primitives':1,'collision_materials':1,
        'total_meshes':5,'total_primitives':5,'total_materials':3,
        'visible_triangles':sum(v for k,v in counts.items() if not k.startswith('COL_')),
        'total_triangles':sum(counts.values()),'triangles_by_mesh':counts},
    'export_primitive_indices_in_node_order':primitive_order,
    'render_indices_after_collision_removal':render_order,
    'transforms_glb':transforms,
    'muzzles':[{'name':'MUZZLE_01','forward_relative_to_root':[0,0,-1],
        'up_relative_to_root':[0,1,0],'offset_ahead_of_lens_m':.02}],
    'groups':{'base_mesh':'Fixed foot, stepped orange pedestal, four steel braces and bolts.',
        'yaw_mesh':'Wide turntable and orange platform supporting both clevis feet; bearings relocated underneath the reactor housing.',
        'pitch_mesh':'Reactor housing/frame, cooling neck, emitter shell/rim, carry handle and trunnion axle.',
        'energy_mesh':'Rear faceted cyan reactor crystal and front lens; both follow pitch.'},
    'motion':{'yaw_axis_glb':[0,1,0],'yaw_degrees':[0,360],
        'pitch_axis_glb':[1,0,0],'pitch_elevation_degrees':[-12,20],'positive_pitch':'raises muzzle',
        'recoil_travel_m':0,'intentional_contacts':['Trunnion axle inside the stationary bearings and solid reactor housing; cooling neck remains clear.'],
        'pose_validation':{'poses_checked':len(poses),'yaw_step_degrees_at_extremes_and_neutral':1,
            'yaw_samples_for_full_pitch':list(range(0,360,45)),'pitch_step_degrees':1,
            'surface_intersections':0,'minimum_moving_height_m':round(min_z,6),
            'maximum_moving_height_m':round(max_z,6),'maximum_horizontal_radius_m':round(max_radius,6)}},
    'palette':{'path':'assets/models/buildings/Textures/colormap.png','dimensions':[512,512],
        'sha256':hashlib.sha256(png).hexdigest(),'embedded_glb':True,'packed_blend':True,
        'license':'Kenney Tower Defense Kit 2.1, CC0','license_file':'art/references/kenney_tower_defense/LICENSE.txt'},
    'geometry_source':'Original project geometry based on the user concept; kit palette reused.',
    'energy_material':{'standard_gltf_emissive_factor':[.65,.65,.65],
        'note':'Base Color stays cyan without emission. Actual beam, glow and firing effects require game VFX integration.'},
    'levels':{'count':1,'kind':'complete base model','overlays':[]},
    'integration':{'rig_connected':False,'playtested':False,'loader_checked':False,
        'note':'Export and fresh import checked; standalone loader probe runs separately. Gameplay, aiming and beam/VFX remain unconnected.'},
    'validation':{'raw_glb':True,'fresh_import':True,'editable_blend_checked':True,
        'normals_uv_manifold':True,'pose_sweep':True,
        'supported_clevis_corners':supported_corners,'axle_clear_of_cooling_neck':True,'embedded_palette_byte_identical':True,
        'glb_sha256':hashlib.sha256(blob).hexdigest()}}
(ROOT/'art/models/laser_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print('LASER_VALIDATED',json.dumps(manifest['counts']),hashlib.sha256(png).hexdigest())


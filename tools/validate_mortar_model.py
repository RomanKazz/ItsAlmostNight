"""Fresh Blender import, raw GLB checks and mortar manifest.

Blender --factory-startup --background --python-exit-code 1 --python tools/validate_mortar_model.py
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
PATH=ROOT/'assets/models/buildings/mortar.glb'
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
expected={'asset_root','base_mesh','COL_BOX_BODY','MUZZLE_01','socket_base','socket_ui_top','yaw_pivot','pitch_pivot','yaw_mesh','pitch_mesh'}
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
    if n['name']!='MUZZLE_01':
        assert max(abs(c) for row in (transform(n)-Matrix.Translation(Vector(n.get('translation',[0,0,0])))) for c in row)<1e-5
    if n['name']!='asset_root':
        expected_parent={'yaw_mesh':'yaw_pivot','pitch_pivot':'yaw_pivot','pitch_mesh':'pitch_pivot','MUZZLE_01':'pitch_pivot','MUZZLE_02':'pitch_pivot','MUZZLE_03':'pitch_pivot','MUZZLE_04':'pitch_pivot'}.get(n['name'],'asset_root')
        assert parents[i]==by_name[expected_parent]
muzzle_positions=[(0,.870+.480/math.sqrt(2),-.480/math.sqrt(2))]
for number,position in enumerate(muzzle_positions,1):close(world[by_name['MUZZLE_%02d'%number]].translation,position)
close(world[by_name['MUZZLE_01']].to_3x3()@Vector((0,0,-1)),(0,1/math.sqrt(2),-1/math.sqrt(2)))
close(world[by_name['MUZZLE_01']].to_3x3()@Vector((0,1,0)),(0,1/math.sqrt(2),1/math.sqrt(2)))
close(world[by_name['socket_ui_top']].translation,(0,1.550,0))
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
            assert min(abs(u-col) for col in (.09375,.34375,.84375,.59375,.46875))<1e-5
            bv=1-v
            assert .025-1e-5<=bv<=.225+1e-5 or .275-1e-5<=bv<=.475+1e-5
assert render_index==4 and len(visible_materials)==1 and len(doc['materials'])==2
for index in visible_materials:
    mat=doc['materials'][index]['pbrMetallicRoughness']
    assert mat.get('metallicFactor',1)==0 and mat.get('roughnessFactor',1)==1 and 'baseColorTexture' in mat
def bounds(points):
    lo=[min(v[i] for v in points) for i in range(3)];hi=[max(v[i] for v in points) for i in range(3)]
    return {'min':lo,'max':hi,'size':[hi[i]-lo[i] for i in range(3)]}
vb=bounds(visible);cb=bounds(collider)
close(vb['min'],(-.6,0,-.6));close(vb['max'],(.6,.870+.73375/math.sqrt(2),.6))
close(cb['min'],(-.6,0,-.6));close(cb['max'],(.6,1.450,.6))


# Inspect editable source and exercise its complete aiming assembly.
with bpy.data.libraries.load(str(ROOT/'art/models/mortar.blend'),link=False) as (source,target):
    assert source.scenes==['MORTAR'];target.scenes=['MORTAR']
editable=target.scenes[0];startup=bpy.context.scene
bpy.context.window.scene=editable;bpy.context.view_layer.update()
assert {c.name for c in editable.collection.children}=={'MORTAR_MODEL','MORTAR_COLLISION','MORTAR_STUDIO'}
assert editable.unit_settings.scale_length==1
assert all(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*',o.name) for o in editable.objects)
assert len([o for o in editable.objects if o.type=='MESH'])==4
assert editable.objects['COL_BOX_BODY'].hide_render
im=next(n.image for n in editable.objects['base_mesh'].data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE')
assert im.packed_file and bytes(im.packed_file.data)==png
for o in editable.objects:
    if o.type in ('MESH','EMPTY'):
        close(o.scale,(1,1,1))
        close(o.rotation_euler,(math.pi/4,0,0) if o.name=='MUZZLE_01' else (0,0,0))
for number,(x,y,z) in enumerate(muzzle_positions,1):
    close(editable.objects['MUZZLE_%02d'%number].matrix_world.translation,(x,-z,y))
def tree(obj,exclude=None):
    obj.data.calc_loop_triangles()
    faces=[tuple(t.vertices) for t in obj.data.loop_triangles]
    if exclude:
        groups={obj.vertex_groups[name].index for name in exclude}
        omitted={v.index for v in obj.data.vertices if any(g.group in groups for g in v.groups)}
        faces=[f for f in faces if not all(v in omitted for v in f)]
    return BVHTree.FromPolygons([obj.matrix_world@v.co for v in obj.data.vertices],faces,all_triangles=True)
base=editable.objects['base_mesh'];mount=editable.objects['yaw_mesh']
head=editable.objects['pitch_mesh']
yaw=editable.objects['yaw_pivot'];pitch=editable.objects['pitch_pivot'];col=editable.objects['COL_BOX_BODY']
# Regression checks for the user's support and bolt fixes.
def group_ids(obj,name):
    index=obj.vertex_groups[name].index
    return {v.index for v in obj.data.vertices if any(g.group==index for g in v.groups)}
def group_tree(obj,name):
    selected=group_ids(obj,name);obj.data.calc_loop_triangles()
    faces=[tuple(t.vertices) for t in obj.data.loop_triangles if all(i in selected for i in t.vertices)]
    return BVHTree.FromPolygons([obj.matrix_world@v.co for v in obj.data.vertices],faces,all_triangles=True)
platform=group_tree(mount,'mortar_yoke_platform');supported_vertices=0
for side in ('left','right'):
    vertices=[mount.matrix_world@mount.data.vertices[i].co for i in group_ids(mount,'mortar_yoke_'+side)]
    bottom=min(v.z for v in vertices)
    for v in vertices:
        if abs(v.z-bottom)>1e-5:continue
        hit,normal,index,distance=platform.ray_cast(Vector((v.x,v.y,.600)),Vector((0,0,-1)),.1)
        assert hit is not None and hit.z>=bottom,'unsupported clevis foot'
        supported_vertices+=1
exposed_bolt_vertices=0
for number in range(4):
    angle=math.pi/4+number*math.pi/2
    radial=Vector((math.cos(angle),math.sin(angle),0))
    brace=group_tree(base,'mortar_base_brace_%d'%number)
    vertices=[base.matrix_world@base.data.vertices[i].co for i in group_ids(base,'mortar_base_bolt_%d'%number)]
    front=max(v.dot(radial) for v in vertices)
    for v in vertices:
        if abs(v.dot(radial)-front)>1e-5:continue
        hit,normal,index,distance=brace.ray_cast(v+radial*.020,-radial,.15)
        assert hit is not None and distance>.0201,'bolt face buried in brace'
        exposed_bolt_vertices+=1
assert supported_vertices==8 and exposed_bolt_vertices==32
base_matrix=base.matrix_world.copy();col_matrix=col.matrix_world.copy();base_tree=tree(base)
poses=sorted(set((a,p) for a in range(360) for p in (-10,0,30)) |
             set((a,p) for a in range(0,360,45) for p in range(-10,31)))
overlaps=[];min_z=math.inf;max_z=-math.inf;max_radius=0
for angle,elevation in poses:
    yaw.rotation_euler.z=math.radians(angle);pitch.rotation_euler.x=math.radians(elevation)
    bpy.context.view_layer.update()
    mount_tree=tree(mount)
    # The drive axle intentionally occupies the bearing; other parts must clear it.
    for obj,exclude in ((head,('mortar_trunnion_left','mortar_trunnion_right')),):
        moving=tree(obj,exclude)
        if moving.overlap(mount_tree) or moving.overlap(base_tree):
            overlaps.append([angle,elevation,obj.name])
    for obj in (mount,head):
        for v in obj.data.vertices:
            p=obj.matrix_world@v.co
            min_z=min(min_z,p.z);max_z=max(max_z,p.z);max_radius=max(max_radius,math.hypot(p.x,p.y))
    assert base.matrix_world==base_matrix and col.matrix_world==col_matrix
    for number in range(1,2):
        socket=editable.objects['MUZZLE_%02d'%number]
        f=socket.matrix_world.to_3x3()@Vector((0,1,0))
        assert abs(f.z-math.sin(math.radians(45+elevation)))<1e-5
        assert socket.parent==pitch
yaw.rotation_euler.z=0;pitch.rotation_euler.x=0;bpy.context.view_layer.update()
print('MORTAR_POSE_OVERLAPS',overlaps[:30],'total',len(overlaps))
assert not overlaps,'aiming assembly intersects fixed structure'
assert min_z>0 and max_z<1.450 and max_radius<.60,'aiming assembly exits collider/footprint'
bpy.context.window.scene=startup
for o in list(editable.objects):bpy.data.objects.remove(o,do_unlink=True)
bpy.data.scenes.remove(editable)

# Fresh GLB import verifies exported geometry independent of source objects.
scene=bpy.data.scenes.new('MORTAR_VALIDATION');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(PATH));bpy.context.view_layer.update()
assert {o.name for o in scene.objects}==expected
for o in scene.objects:
    if o.type!='MESH':continue
    bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    assert all(e.is_manifold for e in bm.edges),'non-manifold '+o.name
    assert bm.calc_volume(signed=True)>0,'inverted '+o.name;bm.free()
    if not o.name.startswith('COL_'):assert o.data.uv_layers.active
for number,(x,y,z) in enumerate(muzzle_positions,1):
    socket=scene.objects['MUZZLE_%02d'%number]
    close(socket.matrix_world.translation,(x,-z,y))
    assert socket.parent==scene.objects['pitch_pivot']
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
    'asset':'mortar','concept':'Revised mortar: short forward-tapered barrel, thick bore walls, narrow reinforcing lip and rounded breech; original support assembly preserved.',
    'files':{'blend':'art/models/mortar.blend','runtime_glb':str(PATH.relative_to(ROOT)),
        'generator':'tools/create_mortar_model.py','revision_script':'tools/refine_mortar_barrel.py','thickness_script':'tools/thicken_mortar_barrel.py','validator':'tools/validate_mortar_model.py',
        'loader_probe':'tools/validate_mortar_loader.cpp',
        'loader_report':'art/models/mortar_loader_report.json','handoff':'art/models/mortar_handoff.md',
        'preview':'art/models/mortar_preview.png','comparison':'art/models/mortar_comparison.png',
        'views':['art/models/mortar_views/'+n+'.png' for n in ('beauty','front','side','back','top')]},
    'authoring':{'skill':'itsalmostnight-models','method':'Reproducible Blender generator and Blender MCP inspection'},
    'barrel_revision':{'date':'2026-10-15','forward_taper':True,'rounded_breech':True,
        'bore_diameter_m':.290,'maximum_outer_diameter_m':.575,'support_assembly_preserved':False,'matching_supports_widened_m':.065},
    'units':'metres','coordinates':'GLB +Y up, -Z forward, +X right; Blender +Z up, +Y forward',
    'visible_bounds_glb':{k:rounded(v) for k,v in vb.items()},
    'colliders':[{'name':'COL_BOX_BODY','type':'box','parent':'asset_root','walkable':False,
        'bounds_glb':{k:rounded(v) for k,v in cb.items()},'unique_vertices':8,'triangles':12,
        'role':'Fixed body and full aiming height envelope; independent of build/movement footprint.'}],
    'footprint':{'recommended_cells':[2,2],'x_range':[-1,1],'z_range':[-1,1],
        'note':'Must be assigned in game code; GLB does not configure build or movement blocking.'},
    'counts':{'visible_meshes':3,'visible_primitives':3,'visible_materials':1,
        'collision_meshes':1,'collision_primitives':1,'collision_materials':1,
        'total_meshes':4,'total_primitives':4,'total_materials':2,
        'visible_triangles':sum(v for k,v in counts.items() if not k.startswith('COL_')),
        'total_triangles':sum(counts.values()),'triangles_by_mesh':counts},
    'export_primitive_indices_in_node_order':primitive_order,
    'render_indices_after_collision_removal':render_order,
    'transforms_glb':transforms,
    'muzzles':[{'name':'MUZZLE_%02d'%i,'position_relative_to_root':rounded(p),
        'forward_relative_to_root':[0,round(1/math.sqrt(2),6),round(-1/math.sqrt(2),6)],
        'up_relative_to_root':[0,round(1/math.sqrt(2),6),round(1/math.sqrt(2),6)],
        'offset_ahead_of_tube_m':.020} for i,p in enumerate(muzzle_positions,1)],
    'groups':{'base_mesh':'Fixed foot, four chunky lugs, tapered pedestal, four diagonal steel braces and bolts.',
        'yaw_mesh':'Steel turntable, orange support platform, two clevis supports and bearing caps.',
        'pitch_mesh':'Short heavy dark mortar tube, thick steel muzzle lip, deep bore floor and two external trunnions.'},
    'motion':{'yaw_axis_glb':[0,1,0],'yaw_degrees':[0,360],
        'pitch_axis_glb':[1,0,0],'pitch_elevation_degrees':[35,75],'neutral_elevation_degrees':45,
        'pitch_pivot_rotation_delta_degrees':[-10,30],'neutral_elevation_baked_into_geometry':True,'positive_pitch':'raises muzzle',
        'recoil_travel_m':0,'intentional_contacts':['Two short side trunnions inside their bearings; no axle crosses the bore.'],
        'pose_validation':{'poses_checked':len(poses),'yaw_step_degrees_at_extremes_and_neutral':1,
            'yaw_samples_for_full_pitch':list(range(0,360,45)),'pitch_step_degrees':1,
            'surface_intersections':0,'minimum_moving_height_m':round(min_z,6),
            'maximum_moving_height_m':round(max_z,6),'maximum_horizontal_radius_m':round(max_radius,6)}},
    'palette':{'path':'assets/models/buildings/Textures/colormap.png','dimensions':[512,512],
        'sha256':hashlib.sha256(png).hexdigest(),'embedded_glb':True,'packed_blend':True,
        'license':'Kenney Tower Defense Kit 2.1, CC0','license_file':'art/references/kenney_tower_defense/LICENSE.txt'},
    'geometry_source':'Original project geometry based on the user concept; kit palette reused.',
    'levels':{'count':1,'kind':'complete base model','overlays':[]},
    'integration':{'rig_connected':False,'playtested':False,'loader_checked':False,
        'note':'Export and fresh import checked; standalone loader probe runs separately. Gameplay, aiming and mortar attack/VFX remain unconnected.'},
    'validation':{'raw_glb':True,'fresh_import':True,'editable_blend_checked':True,
        'normals_uv_manifold':True,'pose_sweep':True,
        'support_feet_checked':supported_vertices,'exposed_bolt_face_vertices_checked':exposed_bolt_vertices,'embedded_palette_byte_identical':True,
        'glb_sha256':hashlib.sha256(blob).hexdigest()}}
(ROOT/'art/models/mortar_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print('MORTAR_VALIDATED',json.dumps(manifest['counts']),hashlib.sha256(png).hexdigest())


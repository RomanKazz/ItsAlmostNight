"""Fresh Blender import, raw GLB checks and Wind blower manifest.

Blender --factory-startup --background --python-exit-code 1 --python tools/validate_wind_blower_model.py
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
from mathutils.kdtree import KDTree

ROOT=Path(__file__).resolve().parents[1]
PATH=ROOT/'assets/models/buildings/wind_blower.glb'
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
expected={'asset_root','base_mesh','yaw_mesh','rotor_mesh','yaw_pivot','rotor_pivot','COL_BOX_BODY','socket_base','socket_ui_top','MUZZLE_01'}
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
        parent={'yaw_mesh':'yaw_pivot','rotor_pivot':'yaw_pivot','rotor_mesh':'rotor_pivot','MUZZLE_01':'yaw_pivot'}.get(n['name'],'asset_root')
        assert parents[i]==by_name[parent]
close(world[by_name['socket_ui_top']].translation,(0,1.430,0))
close(world[by_name['MUZZLE_01']].translation,(0,.960,-.479))
close(world[by_name['MUZZLE_01']].to_3x3()@Vector((0,0,-1)),(0,0,-1))
close(world[by_name['MUZZLE_01']].to_3x3()@Vector((0,1,0)),(0,1,0))
close(world[by_name['yaw_pivot']].translation,(0,.350,0))
close(world[by_name['rotor_pivot']].translation,(0,.960,-.300))
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
            assert min(abs(u-col) for col in (.09375,.84375,.59375,.46875))<1e-5
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
close(vb['min'],(-.600,0,-.600));close(vb['max'],(.600,1.315,.600))
close(cb['min'],(-.610,0,-.610));close(cb['max'],(.610,1.340,.610))

with bpy.data.libraries.load(str(ROOT/'art/models/wind_blower.blend'),link=False) as (source,target):
    assert source.scenes==['WIND_BLOWER'];target.scenes=['WIND_BLOWER']
editable=target.scenes[0];startup=bpy.context.scene;bpy.context.window.scene=editable;bpy.context.view_layer.update()
assert {c.name for c in editable.collection.children}=={'WIND_BLOWER_MODEL','WIND_BLOWER_COLLISION','WIND_BLOWER_STUDIO'}
assert editable.unit_settings.scale_length==1
assert all(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*',o.name) for o in editable.objects)
for o in editable.objects:
    if o.type in ('MESH','EMPTY'):
        close(o.scale,(1,1,1));close(o.rotation_euler,(0,0,0))
assert editable.objects['COL_BOX_BODY'].hide_render
base=editable.objects['base_mesh'];mount=editable.objects['yaw_mesh'];rotor=editable.objects['rotor_mesh']
yaw=editable.objects['yaw_pivot'];spin=editable.objects['rotor_pivot']
im=next(n.image for n in base.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE')
assert im.packed_file and bytes(im.packed_file.data)==png
from mathutils.bvhtree import BVHTree
def ids(obj,name):
    g=obj.vertex_groups[name].index
    return {v.index for v in obj.data.vertices if any(w.group==g for w in v.groups)}
def tree(obj,only=None,exclude=None):
    obj.data.calc_loop_triangles();chosen=ids(obj,only) if only else None
    omitted=ids(obj,exclude) if exclude else set()
    faces=[tuple(t.vertices) for t in obj.data.loop_triangles if (chosen is None or all(i in chosen for i in t.vertices)) and not all(i in omitted for i in t.vertices)]
    return BVHTree.FromPolygons([obj.matrix_world@v.co for v in obj.data.vertices],faces,all_triangles=True)
visible_bolt_vertices=0
for i in range(4):
    a=math.pi/4+i*math.pi/2;radial=Vector((math.cos(a),math.sin(a),0))
    brace=tree(base,only='wind_base_brace_%02d'%i)
    points=[base.matrix_world@base.data.vertices[j].co for j in ids(base,'wind_base_bolt_%02d'%i)]
    front=max(p.dot(radial) for p in points)
    for p in points:
        if abs(p.dot(radial)-front)>1e-5:continue
        hit,normal,index,distance=brace.ray_cast(p+radial*.020,-radial,.15)
        assert hit is not None and distance>.0201,'bolt buried'
        visible_bolt_vertices+=1
assert visible_bolt_vertices==32
# All four bottom corners of the U-yoke are physically seated on the platform.
platform=tree(mount,only='wind_yoke_seat')
pts=[mount.matrix_world@mount.data.vertices[j].co for j in ids(mount,'wind_u_yoke')]
bottom=min(v.z for v in pts);supported=0
for p in pts:
    if abs(p.z-bottom)>1e-5:continue
    hit,*_=platform.ray_cast(Vector((p.x,p.y,.600)),Vector((0,0,-1)),.2)
    assert hit is not None and hit.z>=p.z,'floating U-yoke'
    supported+=1
assert supported==4
# Analytic containment complements the BVH surface checks inside the polygonal bore.
blade_radius=0
for i in range(4):
    for j in ids(rotor,'wind_blade_%02d'%i):
        v=rotor.data.vertices[j].co
        blade_radius=max(blade_radius,math.hypot(v.x,v.z))
assert blade_radius<.262*math.cos(math.pi/12)
poses=sorted(set((a,p) for a in range(360) for p in (0,30,60)) |
             set((a,p) for a in range(0,360,45) for p in range(360)))
base_tree=tree(base);base_matrix=base.matrix_world.copy();overlaps=[];max_radius=0
for angle,phase in poses:
    yaw.rotation_euler.z=math.radians(angle);spin.rotation_euler.y=math.radians(phase)
    bpy.context.view_layer.update()
    rotor_tree=tree(rotor,exclude='wind_rotor_shaft')
    if rotor_tree.overlap(tree(mount)) or rotor_tree.overlap(base_tree):overlaps.append((angle,phase,'rotor'))
    if tree(mount,exclude='wind_turntable').overlap(base_tree):overlaps.append((angle,phase,'mount'))
    for obj in (mount,rotor):
        for v in obj.data.vertices:
            p=obj.matrix_world@v.co;max_radius=max(max_radius,math.hypot(p.x,p.y))
            assert -.610<p.x<.610 and -.610<p.y<.610 and 0<p.z<1.340
    assert base.matrix_world==base_matrix
    # The wind outlet follows yaw, independent of rotor phase.
    muzzle=editable.objects['MUZZLE_01']
    forward=muzzle.matrix_world.to_3x3()@Vector((0,1,0))
    close(forward,(-math.sin(math.radians(angle)),math.cos(math.radians(angle)),0))
yaw.rotation_euler.z=0;spin.rotation_euler.y=0;bpy.context.view_layer.update()
print('WIND_BLOWER_SWEEP_OVERLAPS',overlaps[:10],len(overlaps),'poses',len(poses))
assert not overlaps,'moving fan clips its structure'
source_groups={o.name:len(o.vertex_groups) for o in (base,mount,rotor)}
bpy.context.window.scene=startup
for o in list(editable.objects):bpy.data.objects.remove(o,do_unlink=True)
bpy.data.scenes.remove(editable)
scene=bpy.data.scenes.new('WIND_BLOWER_VALIDATION');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(PATH));bpy.context.view_layer.update()
assert {o.name for o in scene.objects}==expected
for o in scene.objects:
    if o.type!='MESH':continue
    bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    assert all(e.is_manifold for e in bm.edges),'non-manifold '+o.name
    assert bm.calc_volume(signed=True)>0,'inverted '+o.name;bm.free()
    if not o.name.startswith('COL_'):assert o.data.uv_layers.active
close(scene.objects['MUZZLE_01'].matrix_world.translation,(0,.479,.960))
def rounded(v):return [round(float(c),6) for c in v]
transforms={n['name']:{'parent':nodes[parents[i]]['name'] if parents[i] is not None else None,
    'local_translation':rounded(n.get('translation',[0,0,0])),
    'local_rotation_xyzw':rounded(n.get('rotation',[0,0,0,1])),
    'local_scale':rounded(n.get('scale',[1,1,1])),
    'position_relative_to_asset_root':rounded(world[i].translation)} for i,n in enumerate(nodes)}
render_order={name:j for j,name in enumerate(n for n in primitive_order if not n.startswith('COL_'))}
manifest={
 'asset':'wind_blower','concept':'Original imagegen concept followed by real Blender geometry: four swept steel fan blades, orange hub and short duct, thick protective ring, orange U-yoke and rear motor grille.',
 'files':{'blend':'art/models/wind_blower.blend','runtime_glb':str(PATH.relative_to(ROOT)),
    'concept':'art/concepts/wind_blower_concept.png','concept_prompt':'art/concepts/wind_blower_concept_prompt.txt',
    'generator':'tools/create_wind_blower_model.py','validator':'tools/validate_wind_blower_model.py',
    'loader_probe':'tools/validate_wind_blower_loader.cpp','loader_report':'art/models/wind_blower_loader_report.json',
    'handoff':'art/models/wind_blower_handoff.md','preview':'art/models/wind_blower_preview.png',
    'comparison':'art/models/wind_blower_comparison.png',
    'views':['art/models/wind_blower_views/'+n+'.png' for n in ('beauty','front','side','back','top')]},
 'authoring':{'skill':'itsalmostnight-models','concept_tool':'built-in imagegen','method':'Original generated concept, reproducible Blender generator, Blender MCP inspection and final model opening.'},
 'units':'metres','coordinates':'GLB +Y up, -Z forward, +X right; Blender +Z up, +Y forward',
 'visible_bounds_glb':{k:rounded(v) for k,v in vb.items()},
 'colliders':[{'name':'COL_BOX_BODY','type':'box','parent':'asset_root','walkable':False,
    'bounds_glb':{k:rounded(v) for k,v in cb.items()},'unique_vertices':8,'triangles':12}],
 'footprint':{'recommended_cells':[2,2],'x_range':[-1,1],'z_range':[-1,1],
    'note':'Game code assigns footprint and movement blocking independently of GLB collision.'},
 'counts':{'visible_meshes':3,'visible_primitives':3,'visible_materials':1,'collision_meshes':1,
    'collision_primitives':1,'collision_materials':1,'total_meshes':4,'total_primitives':4,'total_materials':2,
    'visible_triangles':sum(v for k,v in counts.items() if not k.startswith('COL_')),
    'total_triangles':sum(counts.values()),'triangles_by_mesh':counts,'editable_vertex_groups':source_groups},
 'export_primitive_indices_in_node_order':primitive_order,'render_indices_after_collision_removal':render_order,
 'transforms_glb':transforms,
 'muzzles':[{'name':'MUZZLE_01','parent':'yaw_pivot','local_translation':[0,.610,-.479],
    'position_relative_to_root':[0,.960,-.479],'forward':[0,0,-1],'up':[0,1,0],
    'note':'Airflow outlet, 0.020 m in front of rim-bolt envelope; does not spin with blades.'}],
 'groups':{'base_mesh':'Fixed foot, orange tapered pedestal, four steel braces and bolts.',
    'yaw_mesh':'Yaw turntable, U-yoke, side mounts, short hollow fan duct/liner/rim, motor cap/grille and rim fasteners.',
    'rotor_mesh':'Four pitched swept blades, central shaft, orange hub and central bolt.'},
 'motion':{'yaw_axis_glb':[0,1,0],'yaw_degrees':[0,360],'rotor_axis_glb':[0,0,-1],
    'rotor_degrees':[0,360],'pitch_degrees':[0,0],'recoil_travel_m':0,
    'note':'Horizontal yaw only; spin about local GLB -Z (Blender +Y). No animation clips or runtime integration.',
    'intentional_contacts':['Rotor shaft runs inside the rear motor bearing; turntable seats on the pedestal.'],
    'validation':{'poses':len(poses),'full_yaw_step_degrees':1,'full_rotor_step_degrees_at_eight_yaws':1,
        'surface_intersections_excluding_bearings':0,'blade_tip_radius_m':round(blade_radius,6),
        'minimum_bore_inradius_m':round(.262*math.cos(math.pi/12),6),'maximum_moving_radius_m':round(max_radius,6)}},
 'palette':{'path':'assets/models/buildings/Textures/colormap.png','dimensions':[512,512],
    'sha256':hashlib.sha256(png).hexdigest(),'embedded_glb':True,'packed_blend':True,
    'license':'Kenney Tower Defense Kit 2.1, CC0','license_file':'art/references/kenney_tower_defense/LICENSE.txt'},
 'geometry_source':'Original project geometry based on the generated concept; kit palette reused.',
 'levels':{'count':1,'kind':'complete base model','overlays':[]},
 'integration':{'loader_checked':False,'rig_connected':False,'wind_attack_connected':False,'playtested':False,
    'note':'Export, motion sweep and fresh import verified. Gameplay, push force, spin/yaw controls and airflow VFX are unconnected.'},
 'validation':{'raw_glb':True,'fresh_import':True,'editable_blend_checked':True,'normals_uv_manifold':True,
    'full_motion_checked':True,'exposed_bolt_face_vertices_checked':visible_bolt_vertices,'supported_yoke_corners':supported,
    'embedded_palette_byte_identical':True,'glb_sha256':hashlib.sha256(blob).hexdigest()}}
(ROOT/'art/models/wind_blower_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print('WIND_BLOWER_VALIDATED',json.dumps(manifest['counts']))

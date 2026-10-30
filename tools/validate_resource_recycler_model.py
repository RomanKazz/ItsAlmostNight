"""Fresh Blender import, raw GLB checks and Resource recycler manifest.

Blender --factory-startup --background --python-exit-code 1 --python tools/validate_resource_recycler_model.py
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
PATH=ROOT/'assets/models/buildings/resource_recycler.glb'
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
expected={'asset_root','base_mesh','indicator_mesh','COL_BOX_BODY','socket_base','socket_ui_top','socket_input','socket_output','socket_status',*('roller_%02d_%s'%(i,suffix) for i in (1,2) for suffix in ('mesh','pivot'))}
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
        parent=n['name'].replace('_mesh','_pivot') if n['name'].startswith('roller_') and n['name'].endswith('_mesh') else 'asset_root'
        assert parents[i]==by_name[parent]
close(world[by_name['socket_ui_top']].translation,(0,1.420,0))
close(world[by_name['socket_input']].translation,(0,1.310,0))
close(world[by_name['socket_output']].translation,(0,.235,-.775))
close(world[by_name['socket_status']].translation,(0,.580,-.410))
for i,y in enumerate((.112,-.112),1):close(world[by_name['roller_%02d_pivot'%i]].translation,(0,.795,-y))
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
            assert min(abs(u-col) for col in (.84375,.59375,.46875,.53125))<1e-5
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
close(vb['min'],(-.607,0,-.770));close(vb['max'],(.607,1.324,.52996242))
close(cb['min'],(-.610,0,-.790));close(cb['max'],(.610,1.350,.560))

# Inspect the authoring scene, including real connected geometry and roller clearance.
with bpy.data.libraries.load(str(ROOT/'art/models/resource_recycler.blend'),link=False) as (source,target):
    assert source.scenes==['RESOURCE_RECYCLER'];target.scenes=['RESOURCE_RECYCLER']
editable=target.scenes[0];startup=bpy.context.scene;bpy.context.window.scene=editable;bpy.context.view_layer.update()
assert {c.name for c in editable.collection.children}=={'RESOURCE_RECYCLER_MODEL','RESOURCE_RECYCLER_COLLISION','RESOURCE_RECYCLER_STUDIO'}
assert editable.unit_settings.scale_length==1
assert all(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*',o.name) for o in editable.objects)
for o in editable.objects:
    if o.type in ('MESH','EMPTY'):
        close(o.scale,(1,1,1));close(o.rotation_euler,(0,0,0))
assert editable.objects['COL_BOX_BODY'].hide_render
base=editable.objects['base_mesh'];indicator=editable.objects['indicator_mesh']
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
    brace=tree(base,only='recycler_foot_%02d'%i)
    points=[base.matrix_world@base.data.vertices[j].co for j in ids(base,'recycler_bolt_%02d'%i)]
    front=max(p.dot(radial) for p in points)
    for p in points:
        if abs(p.dot(radial)-front)>1e-5:continue
        hit,normal,index,distance=brace.ray_cast(p+radial*.020,-radial,.15)
        assert hit is not None and distance>.0201,'bolt buried'
        visible_bolt_vertices+=1
assert visible_bolt_vertices==32
# The hopper has a genuine feed opening; its centre ray reaches the roller mechanism.
hopper=tree(base,only='recycler_hopper')
assert hopper.ray_cast(Vector((0,0,2)),Vector((0,0,-1)),2)[0] is None,'hopper is capped'
# The indicator is visible from the front.
assert tree(base).ray_cast(Vector((0,1,.580)),Vector((0,-1,0)),1)[0].y<.405-1e-5
base_tree=tree(base);rollers=[editable.objects['roller_%02d_mesh'%i] for i in (1,2)]
pivots=[editable.objects['roller_%02d_pivot'%i] for i in (1,2)]
max_envelope=0;overlaps=[]
for angle in range(360):
    for sign,pivot in zip((1,-1),pivots):pivot.rotation_euler.x=sign*math.radians(angle)
    bpy.context.view_layer.update()
    moving=[tree(o,exclude='recycler_axle_%02d'%i) for i,o in enumerate(rollers,1)]
    for i,t in enumerate(moving):
        if t.overlap(base_tree):overlaps.append((angle,i,'housing'))
    if moving[0].overlap(moving[1]):overlaps.append((angle,'rollers'))
    for obj in rollers:
        for v in obj.data.vertices:
            p=obj.matrix_world@v.co
            assert -.610<p.x<.610 and -.560<p.y<.790 and 0<p.z<1.350
    # Core radii/axis spacing prevent containment as well as surface intersections.
    for obj in rollers:
        group=ids(obj,obj.name.replace('roller_','recycler_roller_').replace('_mesh',''))
        for j in group:max_envelope=max(max_envelope,math.hypot(obj.data.vertices[j].co.y,obj.data.vertices[j].co.z))
for pivot in pivots:pivot.rotation_euler.x=0
bpy.context.view_layer.update()
print('ROLLER_SWEEP_OVERLAPS',overlaps[:10],len(overlaps))
assert not overlaps,'rotor clips its enclosure'
assert max_envelope<.105 and 2*.105<.224
# Each disconnected authored component is closed, even before export.
source_groups={o.name:len(o.vertex_groups) for o in (base,indicator,*rollers)}
bpy.context.window.scene=startup
for o in list(editable.objects):bpy.data.objects.remove(o,do_unlink=True)
bpy.data.scenes.remove(editable)
scene=bpy.data.scenes.new('RESOURCE_RECYCLER_VALIDATION');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(PATH));bpy.context.view_layer.update()
assert {o.name for o in scene.objects}==expected
for o in scene.objects:
    if o.type!='MESH':continue
    bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    assert all(e.is_manifold for e in bm.edges),'non-manifold '+o.name
    assert bm.calc_volume(signed=True)>0,'inverted '+o.name;bm.free()
    if not o.name.startswith('COL_'):assert o.data.uv_layers.active

def rounded(v):return [round(float(c),6) for c in v]
transforms={n['name']:{'parent':nodes[parents[i]]['name'] if parents[i] is not None else None,
    'local_translation':rounded(n.get('translation',[0,0,0])),
    'local_rotation_xyzw':rounded(n.get('rotation',[0,0,0,1])),
    'local_scale':rounded(n.get('scale',[1,1,1])),
    'position_relative_to_asset_root':rounded(world[i].translation)} for i,n in enumerate(nodes)}
render_order={name:j for j,name in enumerate(n for n in primitive_order if not n.startswith('COL_'))}
manifest={
 'asset':'resource_recycler','concept':'User orange/steel resource processor with a genuinely open square hopper, orange corner guards, two crushing rollers, discharge tray and green status light.',
 'files':{'blend':'art/models/resource_recycler.blend','runtime_glb':str(PATH.relative_to(ROOT)),
    'generator':'tools/create_resource_recycler_model.py','validator':'tools/validate_resource_recycler_model.py',
    'loader_probe':'tools/validate_resource_recycler_loader.cpp','loader_report':'art/models/resource_recycler_loader_report.json',
    'handoff':'art/models/resource_recycler_handoff.md','preview':'art/models/resource_recycler_preview.png',
    'comparison':'art/models/resource_recycler_comparison.png',
    'views':['art/models/resource_recycler_views/'+n+'.png' for n in ('beauty','front','side','back','top')]},
 'authoring':{'skill':'itsalmostnight-models','method':'Reproducible Blender generator; Blender MCP inspection and final model opening.'},
 'units':'metres','coordinates':'GLB +Y up, -Z forward, +X right; Blender +Z up, +Y forward',
 'visible_bounds_glb':{k:rounded(v) for k,v in vb.items()},
 'colliders':[{'name':'COL_BOX_BODY','type':'box','parent':'asset_root','walkable':False,
    'bounds_glb':{k:rounded(v) for k,v in cb.items()},'unique_vertices':8,'triangles':12}],
 'footprint':{'recommended_cells':[2,2],'x_range':[-1,1],'z_range':[-1,1],
    'note':'Game code assigns footprint and movement blocking independently of GLB collision.'},
 'counts':{'visible_meshes':4,'visible_primitives':4,'visible_materials':2,'collision_meshes':1,
    'collision_primitives':1,'collision_materials':1,'total_meshes':5,'total_primitives':5,'total_materials':3,
    'visible_triangles':sum(v for k,v in counts.items() if not k.startswith('COL_')),
    'total_triangles':sum(counts.values()),'triangles_by_mesh':counts,'editable_vertex_groups':source_groups},
 'export_primitive_indices_in_node_order':primitive_order,'render_indices_after_collision_removal':render_order,
 'transforms_glb':transforms,'muzzles':[],
 'groups':{'base_mesh':'Fixed closed housing walls, hollow hopper/rim/corners, bearing blocks, four feet/bolts, chute and status bezel.',
    'indicator_mesh':'Green ready light; emissive material separate for runtime status changes.',
    'roller_01_mesh':'Front crusher roller, six broad teeth, axle and left drive cap.',
    'roller_02_mesh':'Rear crusher roller, six broad teeth, axle and right drive cap.'},
 'motion':{'roller_axis_glb':[1,0,0],'roller_degrees':[0,360],'counter_rotation_signs':[1,-1],
    'note':'Local +X rotates the front roller and -X the rear roller to pull material down between them. No animation clips or runtime integration.',
    'intentional_contacts':['Axles pass through bearing bores and connect their drive caps.'],
    'validation':{'samples':360,'step_degrees':1,'surface_intersections_excluding_axles':0,'core_radius_m':round(max_envelope,6),'axis_spacing_m':.224}},
 'palette':{'path':'assets/models/buildings/Textures/colormap.png','dimensions':[512,512],
    'sha256':hashlib.sha256(png).hexdigest(),'embedded_glb':True,'packed_blend':True,
    'license':'Kenney Tower Defense Kit 2.1, CC0','license_file':'art/references/kenney_tower_defense/LICENSE.txt'},
 'energy_material':{'standard_gltf_emissive_factor':[.35,.35,.35],'note':'Green base color comes from the original palette; bloom/light/status changes require game code.'},
 'geometry_source':'Original project geometry adapted from user concept; kit palette reused.',
 'levels':{'count':1,'kind':'complete base model','overlays':[]},
 'integration':{'loader_checked':False,'rig_connected':False,'recycling_connected':False,'playtested':False,
    'note':'Export and fresh import verified. Economy, conversion recipes, processing animation and VFX are unconnected.'},
 'validation':{'raw_glb':True,'fresh_import':True,'editable_blend_checked':True,'normals_uv_manifold':True,
    'roller_full_rotation_checked':True,'exposed_bolt_face_vertices_checked':visible_bolt_vertices,
    'open_hopper_checked':True,'indicator_visible':True,'embedded_palette_byte_identical':True,
    'glb_sha256':hashlib.sha256(blob).hexdigest()}}
(ROOT/'art/models/resource_recycler_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print('RESOURCE_RECYCLER_VALIDATED',json.dumps(manifest['counts']))

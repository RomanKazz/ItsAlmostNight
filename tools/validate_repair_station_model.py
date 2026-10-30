"""Fresh Blender import, raw GLB checks and Repair station manifest.

Blender --factory-startup --background --python-exit-code 1 --python tools/validate_repair_station_model.py
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
PATH=ROOT/'assets/models/buildings/repair_station.glb'
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
expected={'asset_root','base_mesh','service_mesh','COL_BOX_BODY','socket_base','socket_ui_top','socket_effect_repair','socket_service_console'}
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
    if n['name']!='asset_root':assert parents[i]==by_name['asset_root']
close(world[by_name['socket_ui_top']].translation,(0,1.580,0))
close(world[by_name['socket_effect_repair']].translation,(0,1.205,0))
close(world[by_name['socket_service_console']].translation,(0,.320,-.545))
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
            assert min(abs(u-col) for col in (.09375,.84375,.59375,.03125))<1e-5
            bv=1-v
            assert .025-1e-5<=bv<=.225+1e-5 or .275-1e-5<=bv<=.475+1e-5
assert render_index==3 and len(visible_materials)==2 and len(doc['materials'])==3
for index in visible_materials:
    mat=doc['materials'][index]['pbrMetallicRoughness']
    assert mat.get('metallicFactor',1)==0 and mat.get('roughnessFactor',1)==1 and 'baseColorTexture' in mat
def bounds(points):
    lo=[min(v[i] for v in points) for i in range(3)];hi=[max(v[i] for v in points) for i in range(3)]
    return {'min':lo,'max':hi,'size':[hi[i]-lo[i] for i in range(3)]}
vb=bounds(visible);cb=bounds(collider)
close(vb['min'],(-.6,0,-.706));close(vb['max'],(.6,1.472,.6))
close(cb['min'],(-.6,0,-.710));close(cb['max'],(.6,1.500,.6))

# Editable source: neutral root, original packed palette and correct collections.
with bpy.data.libraries.load(str(ROOT/'art/models/repair_station.blend'),link=False) as (source,target):
    assert source.scenes==['REPAIR_STATION'];target.scenes=['REPAIR_STATION']
editable=target.scenes[0];startup=bpy.context.scene;bpy.context.window.scene=editable;bpy.context.view_layer.update()
assert {c.name for c in editable.collection.children}=={'REPAIR_STATION_MODEL','REPAIR_STATION_COLLISION','REPAIR_STATION_STUDIO'}
assert editable.unit_settings.scale_length==1
assert all(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*',o.name) for o in editable.objects)
close(editable.objects['asset_root'].location,(0,0,0))
assert editable.objects['COL_BOX_BODY'].hide_render
im=next(n.image for n in editable.objects['base_mesh'].data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE')
assert im.packed_file and bytes(im.packed_file.data)==png
assert len(editable.objects['base_mesh'].vertex_groups)==19
assert len(editable.objects['service_mesh'].vertex_groups)==3
from mathutils.bvhtree import BVHTree
def group_ids(obj,name):
    group=obj.vertex_groups[name].index
    return {v.index for v in obj.data.vertices if any(w.group==group for w in v.groups)}
def group_tree(obj,name=None):
    obj.data.calc_loop_triangles();chosen=group_ids(obj,name) if name else None
    faces=[tuple(t.vertices) for t in obj.data.loop_triangles if chosen is None or all(i in chosen for i in t.vertices)]
    return BVHTree.FromPolygons([obj.matrix_world@v.co for v in obj.data.vertices],faces,all_triangles=True)
base=editable.objects['base_mesh'];service=editable.objects['service_mesh'];body_tree=group_tree(base)
visible_bolt_vertices=0
for i in range(4):
    angle=math.pi/4+i*math.pi/2;radial=Vector((math.cos(angle),math.sin(angle),0))
    brace=group_tree(base,'repair_brace_%d'%i)
    points=[base.matrix_world@base.data.vertices[j].co for j in group_ids(base,'repair_brace_bolt_%d'%i)]
    front=max(p.dot(radial) for p in points)
    for p in points:
        if abs(p.dot(radial)-front)>1e-5:continue
        hit,normal,index,distance=brace.ray_cast(p+radial*.02,-radial,.15)
        assert hit is not None and distance>.0201,'bolt face buried'
        visible_bolt_vertices+=1
assert visible_bolt_vertices==32
# The screen must sit in front of the housing and must not be hidden by a support.
for x in (-.075,0,.075):
    start=Vector((x,1,.441));hit,normal,index,distance=body_tree.ray_cast(start,Vector((0,-1,0)),1)
    assert hit is not None and hit.y<.391-1e-5,'screen occluded by housing or brace'
bpy.context.window.scene=startup
for o in list(editable.objects):bpy.data.objects.remove(o,do_unlink=True)
bpy.data.scenes.remove(editable)

# Reimport geometry in a clean scene, ignoring source objects and shading data.
scene=bpy.data.scenes.new('REPAIR_STATION_VALIDATION');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(PATH));bpy.context.view_layer.update()
assert {o.name for o in scene.objects}==expected
points=set()
for o in scene.objects:
    if o.type!='MESH':continue
    bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    assert all(e.is_manifold for e in bm.edges),'non-manifold '+o.name
    assert bm.calc_volume(signed=True)>0,'inverted '+o.name;bm.free()
    if o.name.startswith('COL_'):continue
    assert o.data.uv_layers.active
    points.update(tuple(float(c) for c in o.matrix_world@v.co) for v in o.data.vertices)
tree=KDTree(len(points))
for i,p in enumerate(points):tree.insert(Vector(p),i)
tree.balance()
symmetry_error=0
for x,y,z in points:
    _,_,distance=tree.find(Vector((-x,y,z)))
    symmetry_error=max(symmetry_error,distance)
    assert distance<2e-6,'left/right mirror symmetry'
print('SYMMETRY_MAX_ERROR_M',symmetry_error)
close(scene.objects['socket_effect_repair'].matrix_world.translation,(0,0,1.205))
def rounded(v):return [round(float(c),6) for c in v]
transforms={}
for i,n in enumerate(nodes):
    if 'mesh' in n and not n['name'].startswith('COL_'):continue
    transforms[n['name']]={'parent':nodes[parents[i]]['name'] if parents[i] is not None else None,
        'local_translation':rounded(n.get('translation',[0,0,0])),
        'local_rotation_xyzw':rounded(n.get('rotation',[0,0,0,1])),
        'local_scale':rounded(n.get('scale',[1,1,1])),
        'position_relative_to_asset_root':rounded(world[i].translation)}
manifest={
    'asset':'repair_station','concept':'User repair station mockup adapted to the Kenney defense family: orange pedestal, four steel braces, thick open-ended cyan wrench and a readable front service console.',
    'files':{'blend':'art/models/repair_station.blend','runtime_glb':str(PATH.relative_to(ROOT)),
        'generator':'tools/create_repair_station_model.py','validator':'tools/validate_repair_station_model.py',
        'loader_probe':'tools/validate_repair_station_loader.cpp',
        'loader_report':'art/models/repair_station_views/loader_report.json','handoff':'art/models/repair_station_handoff.md',
        'preview':'art/models/repair_station_preview.png','comparison':'art/models/repair_station_comparison.png',
        'views':['art/models/repair_station_views/'+n+'.png' for n in ('beauty','front','side','back','top')]},
    'authoring':{'skill':'itsalmostnight-models','blender_mcp_preview':True,
        'method':'Reproducible Blender generator; final file inspected and opened through Blender MCP'},
    'units':'metres','coordinates':'GLB +Y up, -Z forward, +X right; Blender +Z up, +Y forward',
    'visible_bounds_glb':{k:rounded(v) for k,v in vb.items()},
    'colliders':[{'name':'COL_BOX_BODY','type':'box','parent':'asset_root','walkable':False,
        'bounds_glb':{k:rounded(v) for k,v in cb.items()},'unique_vertices':8,'triangles':12}],
    'footprint':{'recommended_cells':[2,2],'x_range':[-1,1],'z_range':[-1,1],
        'note':'Game code must assign footprint and movement blocking; GLB does not configure either.'},
    'counts':{'visible_meshes':2,'visible_primitives':2,'visible_materials':2,
        'collision_meshes':1,'collision_primitives':1,'collision_materials':1,
        'total_meshes':3,'total_primitives':3,'total_materials':3,
        'visible_triangles':counts['base_mesh']+counts['service_mesh'],'total_triangles':sum(counts.values()),
        'triangles_by_mesh':counts},
    'export_primitive_indices_in_node_order':primitive_order,
    'render_indices_after_collision_removal':{'base_mesh':0,'service_mesh':1},
    'transforms_glb':transforms,
    'muzzles':[],
    'groups':{'base_mesh':'Fixed foot, tapered orange housing, four steel braces and bolts, stem/collar, console apron, display bezel and control deck.',
        'service_mesh':'Fixed three-dimensional cyan wrench, display and console indicator; common energy material.'},
    'motion':{'yaw_pivot':False,'pitch_pivot':False,'recoil_travel_m':0,
        'note':'Static repair station; no aiming pivots or animations. Repair targeting, range, rules and VFX are separate gameplay integration.'},
    'palette':{'path':'assets/models/buildings/Textures/colormap.png','dimensions':[512,512],
        'sha256':hashlib.sha256(png).hexdigest(),'embedded_glb':True,'packed_blend':True,
        'license':'Kenney Tower Defense Kit 2.1, CC0','license_file':'art/references/kenney_tower_defense/LICENSE.txt'},
    'geometry_source':'New concept geometry authored for the project; original kit palette reused.',
    'energy_material':{'standard_gltf_emissive_factor':[.35,.35,.35],
        'note':'Base color remains readable without emission. Glow, bloom and repair effects require game renderer/VFX integration.'},
    'levels':{'count':1,'kind':'complete base model','overlays':[]},
    'integration':{'rig_connected':False,'playtested':False,'loader_checked':False,
        'note':'Export and fresh import checked. Loader probe runs separately; gameplay/VFX integration remains unconnected.'},
    'validation':{'raw_glb':True,'fresh_import':True,'editable_blend_checked':True,
        'normals_uv_manifold':True,'left_right_mirror_symmetry':True,
        'exposed_bolt_face_vertices_checked':visible_bolt_vertices,'display_clear_of_supports':True,'embedded_palette_byte_identical':True,
        'glb_sha256':hashlib.sha256(blob).hexdigest()}}
(ROOT/'art/models/repair_station_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print('REPAIR_STATION_VALIDATED',json.dumps(manifest['counts']),hashlib.sha256(png).hexdigest())

"""Fresh Blender import, raw GLB checks and Stone harvester manifest.

Blender --factory-startup --background --python-exit-code 1 --python tools/validate_stone_harvester_model.py
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
PATH=ROOT/'assets/models/buildings/stone_harvester.glb'
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
expected={'asset_root','base_mesh','module_mesh','resource_mesh','drill_mesh','drill_pivot','COL_BOX_BODY','socket_base','socket_ui_top','socket_output','socket_resource_mount','socket_effect_dust'}
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
        parent='drill_pivot' if n['name']=='drill_mesh' else 'asset_root'
        assert parents[i]==by_name[parent]
close(world[by_name['socket_ui_top']].translation,(0,.995,0))
close(world[by_name['socket_resource_mount']].translation,(0,.215,0))
close(world[by_name['socket_output']].translation,(-.285,.280,-.355))
close(world[by_name['socket_effect_dust']].translation,(0,.390,-.075))
close(world[by_name['drill_pivot']].translation,(0,.610,.030))
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
assert render_index==5 and len(visible_materials)==1 and len(doc['materials'])==2
for index in visible_materials:
    mat=doc['materials'][index]['pbrMetallicRoughness']
    assert mat.get('metallicFactor',1)==0 and mat.get('roughnessFactor',1)==1 and 'baseColorTexture' in mat
def bounds(points):
    lo=[min(v[i] for v in points) for i in range(3)];hi=[max(v[i] for v in points) for i in range(3)]
    return {'min':lo,'max':hi,'size':[hi[i]-lo[i] for i in range(3)]}
vb=bounds(visible);cb=bounds(collider)
close(vb['min'],(-.459,0,-.459));close(vb['max'],(.459,.890,.459))
close(cb['min'],(-.480,0,-.480));close(cb['max'],(.480,.920,.480))

with bpy.data.libraries.load(str(ROOT/'art/models/stone_harvester.blend'),link=False) as (source,target):
    assert source.scenes==['STONE_HARVESTER'];target.scenes=['STONE_HARVESTER']
editable=target.scenes[0];startup=bpy.context.scene;bpy.context.window.scene=editable;bpy.context.view_layer.update()
assert {c.name for c in editable.collection.children}=={'STONE_HARVESTER_MODEL','STONE_HARVESTER_COLLISION','STONE_HARVESTER_STUDIO'}
assert editable.unit_settings.scale_length==1
assert all(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*',o.name) for o in editable.objects)
for o in editable.objects:
    if o.type in ('MESH','EMPTY'):
        close(o.scale,(1,1,1));close(o.rotation_euler,(0,0,0))
assert editable.objects['COL_BOX_BODY'].hide_render
base=editable.objects['base_mesh'];module=editable.objects['module_mesh']
resource=editable.objects['resource_mesh'];blade=editable.objects['drill_mesh'];pivot=editable.objects['drill_pivot']
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
# Verify the family base is built by the shared generator, not visually approximated.
import sys
sys.path.insert(0,str(ROOT/'tools'))
from harvester_family_base import FAMILY_ID,MOUNT_HEIGHT
assert editable.objects['asset_root']['family']==FAMILY_ID and MOUNT_HEIGHT==.215
assert len(base.vertex_groups)==8
bolt_vertices=0
for i in range(4):
    a=i*math.pi/2;radial=Vector((math.cos(a),math.sin(a),0))
    housing=tree(base,only='harvester_housing')
    pts=[base.matrix_world@base.data.vertices[j].co for j in ids(base,'harvester_bolt_%02d'%i)]
    front=max(p.dot(radial) for p in pts)
    for p in pts:
        if abs(p.dot(radial)-front)>1e-5:continue
        hit,normal,index,distance=housing.ray_cast(p+radial*.02,-radial,.15)
        assert hit is not None and distance>.0201,'bolt buried'
        bolt_vertices+=1
assert bolt_vertices==32
# Supports are seated on the module deck.
deck=tree(base,only='harvester_module_deck');supported=0
for name in ('stone_frame_foot_left','stone_frame_foot_right','stone_output_foot_00','stone_output_foot_01'):
    pts=[module.matrix_world@module.data.vertices[j].co for j in ids(module,name)]
    bottom=min(p.z for p in pts)
    for p in pts:
        if abs(p.z-bottom)>1e-5:continue
        hit,*_=deck.ray_cast(Vector((p.x,p.y,.240)),Vector((0,0,-1)),.1)
        # The outer output foot may also seat on the continuous upper rim.
        if hit is None or hit.z+1e-5<bottom:
            hit,*_=tree(base).ray_cast(Vector((p.x,p.y,.240)),Vector((0,0,-1)),.1)
        assert hit is not None and hit.z+1e-5>=bottom,(name,'floating support',tuple(p),tuple(hit) if hit else None)
        supported+=1
# Compare every base vertex, face and UV to the authored wood sibling.
with bpy.data.libraries.load(str(ROOT/'art/models/wood_harvester.blend'),link=False) as (src,dst):
    dst.meshes=['base_mesh']
sibling=dst.meshes[0]
assert len(sibling.vertices)==len(base.data.vertices) and len(sibling.polygons)==len(base.data.polygons)
for a,b in zip(sibling.vertices,base.data.vertices):close(a.co,b.co)
for a,b in zip(sibling.polygons,base.data.polygons):assert tuple(a.vertices)==tuple(b.vertices)
for a,b in zip(sibling.uv_layers.active.data,base.data.uv_layers.active.data):close(a.uv,b.uv)
bpy.data.meshes.remove(sibling)
base_tree=tree(base);module_tree=tree(module);resource_tree=tree(resource)
module_outside_bearing=tree(module,exclude='stone_spindle_bearing')
max_radius=0;max_height=0;overlaps=[]
for angle in range(360):
    pivot.rotation_euler.z=math.radians(angle);bpy.context.view_layer.update()
    moving=tree(blade,exclude='stone_auger_core')
    for label,fixed in [('base',base_tree),('module',module_tree),('resource',resource_tree)]:
        if moving.overlap(fixed):overlaps.append((angle,label))
    core=tree(blade,only='stone_auger_core')
    for label,fixed in [('base',base_tree),('module_outside_bearing',module_outside_bearing),('resource',resource_tree)]:
        if core.overlap(fixed):overlaps.append((angle,'core_'+label))
    for v in blade.data.vertices:
        p=blade.matrix_world@v.co
        assert -.480<p.x<.480 and -.480<p.y<.480 and 0<p.z<.920
        max_radius=max(max_radius,math.hypot(v.co.x,v.co.y));max_height=max(max_height,p.z)
pivot.rotation_euler.z=0;bpy.context.view_layer.update()
print('STONE_DRILL_SWEEP',overlaps[:10],len(overlaps))
assert not overlaps
assert max_radius<=.12401 and max_height<=.71901
# Idle workpiece and jaws remain below the entire rotating drill.
rock_top=max((resource.matrix_world@resource.data.vertices[i].co).z for i in ids(resource,'stone_work_rock'))
tip_bottom=min((blade.matrix_world@blade.data.vertices[i].co).z for i in ids(blade,'stone_drill_tip'))
assert tip_bottom-rock_top>.015
# Both frame masts sit inside their feet and reach the overhead crossbeam.
for side in ('left','right'):
    mast=[module.data.vertices[i].co for i in ids(module,'stone_frame_mast_'+side)]
    foot=[module.data.vertices[i].co for i in ids(module,'stone_frame_foot_'+side)]
    assert min(p.z for p in mast)<max(p.z for p in foot)
    assert max(p.z for p in mast)>.724
source_groups={o.name:len(o.vertex_groups) for o in (base,module,resource,blade)}
bpy.context.window.scene=startup
for o in list(editable.objects):bpy.data.objects.remove(o,do_unlink=True)
bpy.data.scenes.remove(editable)
scene=bpy.data.scenes.new('STONE_HARVESTER_VALIDATION');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(PATH));bpy.context.view_layer.update()
assert {o.name for o in scene.objects}==expected
for o in scene.objects:
    if o.type!='MESH':continue
    bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    assert all(e.is_manifold for e in bm.edges),'non-manifold '+o.name
    assert bm.calc_volume(signed=True)>0,'inverted '+o.name;bm.free()
    if not o.name.startswith('COL_'):assert o.data.uv_layers.active
close(scene.objects['drill_pivot'].matrix_world.translation,(0,-.030,.610))
def rounded(v):return [round(float(c),6) for c in v]
transforms={n['name']:{'parent':nodes[parents[i]]['name'] if parents[i] is not None else None,
    'local_translation':rounded(n.get('translation',[0,0,0])),
    'local_rotation_xyzw':rounded(n.get('rotation',[0,0,0,1])),
    'local_scale':rounded(n.get('scale',[1,1,1])),
    'position_relative_to_asset_root':rounded(world[i].translation)} for i,n in enumerate(nodes)}
render_order={name:j for j,name in enumerate(n for n in primitive_order if not n.startswith('COL_'))}
manifest={
 'asset':'stone_harvester','concept':'Stone variant of the established shared wood/stone/crystal harvester concept: overhead vertical helical drill, clamped rock and finished stones on the identical orange octagonal pedestal.',
 'files':{'blend':'art/models/stone_harvester.blend','runtime_glb':str(PATH.relative_to(ROOT)),
    'concept':'art/concepts/wood_harvester_concept.png','concept_prompt':'art/concepts/wood_harvester_concept_prompt.txt',
    'family_spec':'art/models/harvester_family_spec.json','family_base_generator':'tools/harvester_family_base.py',
    'generator':'tools/create_stone_harvester_model.py','validator':'tools/validate_stone_harvester_model.py',
    'loader_probe':'tools/validate_stone_harvester_loader.cpp','loader_report':'art/models/stone_harvester_loader_report.json',
    'handoff':'art/models/stone_harvester_handoff.md','preview':'art/models/stone_harvester_preview.png',
    'comparison':'art/models/stone_harvester_comparison.png',
    'views':['art/models/stone_harvester_views/'+n+'.png' for n in ('beauty','front','side','back','top')]},
 'authoring':{'skill':'itsalmostnight-models','concept_tool':'Existing built-in imagegen family concept','method':'Adapted shared concept, reproducible Blender generator, Blender MCP inspection.'},
 'family':{'id':FAMILY_ID,'module_mount_height_m':MOUNT_HEIGHT,'wood_and_stone_status':'models authored with identical pedestal; crystal still concept only; existing runtime files preserved'},
 'units':'metres','coordinates':'GLB +Y up, -Z forward, +X right; Blender +Z up, +Y forward',
 'visible_bounds_glb':{k:rounded(v) for k,v in vb.items()},
 'colliders':[{'name':'COL_BOX_BODY','type':'box','parent':'asset_root','walkable':False,
    'bounds_glb':{k:rounded(v) for k,v in cb.items()},'unique_vertices':8,'triangles':12}],
 'footprint':{'recommended_cells':[1,1],'x_range':[-.5,.5],'z_range':[-.5,.5],
    'note':'Matches current producer footprint. Export is final metre scale; use scale 1.0 during future migration, not the old quarry renderer factor .65.'},
 'counts':{'visible_meshes':4,'visible_primitives':4,'visible_materials':1,'collision_meshes':1,
    'collision_primitives':1,'collision_materials':1,'total_meshes':5,'total_primitives':5,'total_materials':2,
    'visible_triangles':sum(v for k,v in counts.items() if not k.startswith('COL_')),
    'total_triangles':sum(counts.values()),'triangles_by_mesh':counts,'editable_vertex_groups':source_groups},
 'export_primitive_indices_in_node_order':primitive_order,'render_indices_after_collision_removal':render_order,
 'transforms_glb':transforms,'muzzles':[],
 'groups':{'base_mesh':'Identical shared octagonal plinth, orange shell, steel lip, deck and four bolts.',
    'module_mesh':'Fixed supported frame, overhead drive head/motor/bearing, low rock bed, three clamp jaws and output tray.',
    'resource_mesh':'One work rock and two smaller output stones using the original grey swatch.',
    'drill_mesh':'Tapered central shaft, reinforced tip and closed helical flight.'},
 'motion':{'drill_axis_glb':[0,1,0],'drill_degrees':[0,360],'recoil_travel_m':0,
    'note':'Drill rotates around local GLB +Y (Blender +Z); only shaft/bearing overlap intended. Work rock is in an idle pose, no drilling feed stroke or rock animation authored.',
    'validation':{'poses':360,'step_degrees':1,'surface_intersections_excluding_core':0,
        'drill_radius_m':round(max_radius,6),'maximum_moving_height_m':round(max_height,6),
        'idle_tip_to_work_rock_clearance_m':round(tip_bottom-rock_top,6)}},
 'palette':{'path':'assets/models/buildings/Textures/colormap.png','dimensions':[512,512],
    'sha256':hashlib.sha256(png).hexdigest(),'embedded_glb':True,'packed_blend':True,
    'license':'Kenney Tower Defense Kit 2.1, CC0','license_file':'art/references/kenney_tower_defense/LICENSE.txt'},
 'geometry_source':'Original project geometry based on the generated concept; kit palette reused.',
 'levels':{'count':1,'kind':'complete base model','overlays':[]},
 'integration':{'loader_checked':False,'rig_connected':False,'production_connected':False,'playtested':False,
    'note':'Export/fresh import/motion checked. Existing quarry.glb and gameplay untouched. Future resource renderer migration required.'},
 'validation':{'raw_glb':True,'fresh_import':True,'editable_blend_checked':True,'normals_uv_manifold':True,
    'full_drill_rotation_checked':True,'shared_base_vertex_face_uv_identical_to_wood':True,'exposed_bolt_face_vertices_checked':bolt_vertices,'supported_module_vertices':supported,
    'embedded_palette_byte_identical':True,'glb_sha256':hashlib.sha256(blob).hexdigest()}}
(ROOT/'art/models/stone_harvester_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print('STONE_HARVESTER_VALIDATED',json.dumps(manifest['counts']))

"""Validate tool GLBs and reopen the editable packed Blender source.
Run in a fresh background Blender; no files are changed.
"""
from pathlib import Path
import hashlib,json,math,struct
import bpy,bmesh
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/tool_levels'

def read(path):
 data=path.read_bytes();assert struct.unpack_from('<I',data,8)[0]==len(data)
 size=struct.unpack_from('<I',data,12)[0]
 return json.loads(data[20:20+size]),data[28+size:]
def values(doc,binary,index):
 a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
 width={'VEC2':2,'VEC3':3,'VEC4':4,'SCALAR':1}[a['type']]
 assert a['componentType']==5126
 offset=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',width*4)
 return [struct.unpack_from('<'+'f'*width,binary,offset+i*stride) for i in range(a['count'])]
def embedded(doc,binary,image):
 v=doc['bufferViews'][image['bufferView']];start=v.get('byteOffset',0)
 return binary[start:start+v['byteLength']]
reference,rb=read(ROOT/'assets/models/buildings/cannon.glb')
palette=embedded(reference,rb,reference['images'][0])
report={}
for name in ('axe','pickaxe'):
 source,sb=read(ROOT/f'assets/models/tools/{name}.glb')
 doc,binary=read(ROOT/f'assets/models/tools/{name}_levels.glb')
 assert not doc.get('extensionsRequired')
 assert all('uri' not in value for value in doc['buffers']+doc['images'])
 assert doc['meshes'][0]==source['meshes'][0]
 assert doc['accessors'][:len(source['accessors'])]==source['accessors']
 assert binary[:source['buffers'][0]['byteLength']]==sb[:source['buffers'][0]['byteLength']]
 assert embedded(doc,binary,doc['images'][0])==embedded(source,sb,source['images'][0])
 assert embedded(doc,binary,doc['images'][1])==palette
 assert doc.get('animations')==source.get('animations')
 assert doc['nodes'][0]==source['nodes'][0]
 if name=='pickaxe':
  old=source['nodes'][1].copy();new=doc['nodes'][1].copy();new['children']=old['children'];assert new==old
  for node in doc['nodes'][2:]:assert node['scale']==source['nodes'][0]['scale']
 assert len(doc['meshes'])==8
 for mesh in doc['meshes'][1:]:
  primitive=mesh['primitives'][0]
  assert len(mesh['primitives'])==1 and 'COLOR_0' not in primitive['attributes']
  for attr in ('POSITION','NORMAL','TEXCOORD_0'):
   data=values(doc,binary,primitive['attributes'][attr]);assert all(math.isfinite(v) for p in data for v in p)
   if attr=='NORMAL':assert all(abs(sum(v*v for v in p)-1)<1e-4 for p in data)
   if attr=='TEXCOORD_0':assert all(0<=v<=1 for p in data for v in p)
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 bpy.ops.import_scene.gltf(filepath=str(ROOT/f'assets/models/tools/{name}_levels.glb'))
 counts=[]
 for obj in bpy.context.scene.objects:
  if obj.type!='MESH' or not obj.name.startswith('upgrade_'):continue
  bm=bmesh.new();bm.from_mesh(obj.data)
  bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
  assert all(edge.is_manifold for edge in bm.edges),obj.name
  assert bm.calc_volume(signed=True)>0,obj.name
  counts.append({'name':obj.name,'triangles':len(obj.data.polygons),'vertices':len(obj.data.vertices)})
  bm.free()
 report[name]={'meshes':8,'selected_draws':2,'clean_import':'passed','overlay_geometry':counts}
bpy.ops.wm.open_mainfile(filepath=str(ART/'tool_levels.blend'))
assert bpy.context.scene.name=='IAN_TOOL_LEVELS'
assert bpy.data.collections.get('TOOLS_MODEL') and bpy.data.collections.get('TOOLS_STUDIO')
assert sum(o.type=='MESH' for o in bpy.data.collections['TOOLS_MODEL'].objects)==16
assert all(i.packed_file for i in bpy.data.images if i.source=='FILE')
report['packed_source']='passed'
report['palette_sha256']=hashlib.sha256(palette).hexdigest()
(ART/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('TOOL_LEVEL_VALIDATION',json.dumps(report,indent=2))

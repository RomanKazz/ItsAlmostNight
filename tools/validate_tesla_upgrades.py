"""Run in fresh background Blender: raw GLB, round-trip topology, UV and symmetry."""
from pathlib import Path
import hashlib,json,math,struct
import bpy,bmesh
from mathutils import Vector
from mathutils.kdtree import KDTree
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/tesla_levels'
manifest=json.loads((ART/'manifest.json').read_text())
blob=(ROOT/'assets/models/buildings/tesla_upgrades.glb').read_bytes()
assert struct.unpack_from('<III',blob)==(0x46546c67,2,len(blob))
n=struct.unpack_from('<I',blob,12)[0];d=json.loads(blob[20:20+n]);binary=blob[28+n:]
assert len(d['meshes'])==7 and len(d['materials'])==1 and len(d['images'])==1
assert not d.get('extensionsRequired') and not d.get('animations') and not d.get('skins')
v=d['bufferViews'][d['images'][0]['bufferView']];start=v.get('byteOffset',0)
assert binary[start:start+v['byteLength']]==(ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()
assert hashlib.sha256((ROOT/'assets/models/buildings/tesla_tower.glb').read_bytes()).hexdigest()==manifest['base_sha256']
def values(index):
 a=d['accessors'][index];v=d['bufferViews'][a['bufferView']]
 fmt={5126:'f',5125:'I',5123:'H',5121:'B'}[a['componentType']]
 width={'SCALAR':1,'VEC2':2,'VEC3':3}[a['type']];row='<'+fmt*width
 start=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',struct.calcsize(row))
 return [struct.unpack_from(row,binary,start+i*stride) for i in range(a['count'])]
for node in d['nodes']:
 assert node.get('translation',[0,0,0])==[0,0,0]
 assert node.get('rotation',[0,0,0,1])==[0,0,0,1]
 assert node.get('scale',[1,1,1])==[1,1,1]
 assert node['name'] in [f'tesla_upgrade_l{i}' for i in range(2,9)]
for m in d['meshes']:
 assert len(m['primitives'])==1
 p=m['primitives'][0];a=p['attributes'];positions=values(a['POSITION']);normals=values(a['NORMAL']);uv=values(a['TEXCOORD_0'])
 assert all(all(math.isfinite(x) for x in v) for v in positions+normals+uv)
 assert all(abs(Vector(v).length-1)<1e-4 for v in normals)
 assert all(min(abs(u-c) for c in (.65625,.96875,.84375,.59375,.03125))<1e-5 for u,v in uv)
 idx=[i[0] for i in values(p['indices'])];seen=set()
 for i in range(0,len(idx),3):
  tri=[Vector(positions[j]) for j in idx[i:i+3]]
  normal=(tri[1]-tri[0]).cross(tri[2]-tri[0]);assert normal.length>1e-9
  assert all(normal.dot(Vector(normals[j]))>0 for j in idx[i:i+3])
  key=tuple(sorted(tuple(round(c,6) for c in v) for v in tri));assert key not in seen
  seen.add(key)
scene=bpy.data.scenes.new('TESLA_UPGRADES_VALIDATION');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/models/buildings/tesla_upgrades.glb'))
report=[]
for obj in scene.objects:
 if obj.type!='MESH':continue
 bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
 assert all(e.is_manifold for e in bm.edges),obj.name
 assert bm.calc_volume(signed=True)>0;bm.free()
 points=[obj.matrix_world@v.co for v in obj.data.vertices]
 tree=KDTree(len(points))
 for i,v in enumerate(points):tree.insert(v,i)
 tree.balance();error=0
 for x,y,z in points:
  for other in ((-y,x,z),(-x,y,z),(x,-y,z)):
   distance=tree.find(Vector(other))[2];error=max(error,distance)
 assert error<2e-6,(obj.name,error)
 assert max(abs(v.x) for v in points)<=.639 and max(abs(v.y) for v in points)<=.639
 assert min(v.z for v in points)>0 and max(v.z for v in points)<1.44
 report.append({'mesh':obj.name,'manifold':True,'normals_uv_valid':True,'symmetry_max_error_m':error})
# Finalize the isolated library as a normal editable project, with a useful opening view.
bpy.ops.wm.open_mainfile(filepath=str(ART/'tesla_levels.blend'))
scene=bpy.data.scenes['TESLA_LEVELS'];bpy.context.window.scene=scene
assert scene.unit_settings.scale_length==1
for obj in scene.objects:
 if obj.name.startswith('tesla_upgrade_l'):
  level=int(obj.name.rsplit('l',1)[1]);obj.hide_render=level!=8;obj.hide_set(level!=8)
  image=next(n.image for n in obj.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE')
  assert image.packed_file and bytes(image.packed_file.data)==(ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()
  assert len(obj.vertex_groups)>0
scene.camera.location=(2.5,4,2.7);scene.camera.rotation_euler=(Vector((0,0,.72))-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.ortho_scale=2.05
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'tesla_levels.blend'),check_existing=False)
(ART/'validation.json').write_text(json.dumps({'meshes':report,'base_unchanged':True,'embedded_palette_exact':True,'source_packed':True,'round_trip_verified':True},indent=2)+'\n')
print('TESLA_UPGRADES_VALIDATED',len(report),'meshes')

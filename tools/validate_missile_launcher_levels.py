from pathlib import Path
import bpy,bmesh,json,struct,math,hashlib
from mathutils import Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/missile_launcher_levels'
bpy.ops.wm.open_mainfile(filepath=str(ART/'missile_launcher_levels.blend'))
report=[]
def tree(obj,exclude=False,bearing=False):
 obj.data.calc_loop_triangles();verts=[obj.matrix_world@v.co for v in obj.data.vertices]
 omitted=set()
 if exclude:
  group=obj.vertex_groups.get('launcher_trunnion_axle')
  if group:omitted={v.index for v in obj.data.vertices if any(g.group==group.index for g in v.groups)}
  else:omitted={v.index for v in obj.data.vertices if math.hypot(v.co.y/1.2,v.co.z)<.041}
 if bearing:omitted={v.index for v in obj.data.vertices if v.co.z<.036}
 tris=[tuple(t.vertices) for t in obj.data.loop_triangles if not all(i in omitted for i in t.vertices)]
 return BVHTree.FromPolygons(verts,tris,all_triangles=True)
for level in range(1,9):
 scene=bpy.data.scenes['MISSILE_LAUNCHER_L%02d'%level];bpy.context.window.scene=scene
 objs={o.get('canonical_name'):o for o in scene.objects if o.get('canonical_name')}
 yaw=objs['yaw_pivot'];pitch=objs['pitch_pivot'];base=objs['base_mesh'];head=objs['pitch_mesh'];mount=objs['yaw_mesh']
 yaw.rotation_mode=pitch.rotation_mode='XYZ';base_tree=tree(base);bad=[];maximum=0
 # Relative pitch clearance is invariant under yaw; test every degree.
 for degree in range(-8,31):
  pitch.rotation_euler.x=math.radians(degree);bpy.context.view_layer.update()
  if tree(head,True).overlap(tree(mount)):bad.append(['pitch',degree])
  maximum=max(maximum,max((head.matrix_world@v.co).z for v in head.data.vertices))
 # Full-yaw sweep at extreme and neutral pitch catches fixed-base contact.
 for degree in range(360):
  yaw.rotation_euler.z=math.radians(degree)
  for elevation in (-8,0,30):
   pitch.rotation_euler.x=math.radians(elevation);bpy.context.view_layer.update()
   if tree(head,True).overlap(base_tree) or tree(mount,bearing=True).overlap(base_tree):bad.append(['yaw',degree,elevation])
 yaw.rotation_euler.z=0;pitch.rotation_euler.x=0;bpy.context.view_layer.update()
 print('LEVEL',level,'CONTACTS',bad[:10],'HEIGHT',maximum,flush=True)
 assert not bad
 report.append({'level':level,'poses':1119,'unintended_motion_contacts':0,'max_moving_height':maximum})
# Raw metadata and fresh imports, one at a time.
for level in range(1,9):
 path=ART/f'missile_launcher_lvl{level:02d}.glb';blob=path.read_bytes();n=struct.unpack_from('<I',blob,12)[0];d=json.loads(blob[20:20+n]);binary=blob[28+n:]
 assert len(d['meshes'])==4 and len(d['images'])==1
 view=d['bufferViews'][d['images'][0]['bufferView']];off=view.get('byteOffset',0)
 assert binary[off:off+view['byteLength']]==(ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()
 assert len({n['name'] for n in d['nodes']})==len(d['nodes'])
 scene=bpy.data.scenes.new('ROUNDTRIP');bpy.context.window.scene=scene
 before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(path));bpy.context.view_layer.update()
 obs={o.name.split('.')[0]:o for o in scene.objects}
 for obj in scene.objects:
  if obj.type!='MESH':continue
  bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
  assert all(e.is_manifold for e in bm.edges),obj.name
  assert bm.calc_volume(signed=True)>0;bm.free()
  assert all(math.isfinite(c) for v in obj.data.vertices for c in v.co)
  assert all(abs(v.normal.length-1)<1e-4 for v in obj.data.vertices)
 for i,(x,z) in enumerate(((-.113,.113),(.113,.113),(-.113,-.113),(.113,-.113)),1):
  socket=obs['MUZZLE_%02d'%i]
  assert (socket.matrix_world.translation-Vector((x,.4388,1.09+z))).length<1e-5
  assert socket.parent==obs['pitch_pivot']
 report[level-1]['round_trip']=True
 for o in list(scene.objects):bpy.data.objects.remove(o,do_unlink=True)
 bpy.data.scenes.remove(scene)
bpy.context.window.scene=bpy.data.scenes['MISSILE_LAUNCHER_L08']
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'missile_launcher_levels.blend'))
(ART/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('ALL_LEVELS_VALIDATED',flush=True)

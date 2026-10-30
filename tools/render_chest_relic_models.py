"""Validate clean GLB imports, render icons and normalize the editable .blend."""
from pathlib import Path
import bpy, math, json, struct
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/chest_relics'
ICONS=ROOT/'assets/ui/relics';ICONS.mkdir(parents=True,exist_ok=True)
source=ART/'chest_relics.blend'
bpy.ops.wm.open_mainfile(filepath=str(source))
scene=next(s for s in bpy.data.scenes if s.name.startswith('Chest Relics Authoring'))
if bpy.context.window:bpy.context.window.scene=scene
bpy.ops.wm.save_as_mainfile(filepath=str(source))
# Independent fresh reimport for every runtime file (no authoring geometry).
scene=bpy.data.scenes.new('Relic Runtime Validation');bpy.context.window.scene=scene
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=256;scene.render.resolution_y=256;scene.render.resolution_percentage=100
scene.render.film_transparent=True
scene.view_settings.view_transform='Standard'
scene.world=bpy.data.worlds.new('Soft studio');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs['Color'].default_value=(.55,.55,.55,1);bg.inputs['Strength'].default_value=.7
camdata=bpy.data.cameras.new('Icon camera');cam=bpy.data.objects.new('Icon camera',camdata);scene.collection.objects.link(cam);scene.camera=cam
camdata.type='ORTHO';camdata.ortho_scale=1.9
for name,pos,power,size in [('Key',(-3,-4,5),250,5),('Fill',(4,-2,2),100,4)]:
 d=bpy.data.lights.new(name,'AREA');o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=pos;d.energy=power;d.shape='DISK';d.size=size;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
report=[]
for path in sorted((ROOT/'assets/models/items/relics').glob('*.glb')):
 before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(path));objs=[o for o in scene.objects if o not in before]
 mesh=[o for o in objs if o.type=='MESH'];assert len(mesh)==1,path
 obj=mesh[0];assert len(obj.data.materials)==1,path
 bounds=[obj.matrix_world@Vector(v) for v in obj.bound_box];lo=Vector([min(v[i] for v in bounds) for i in range(3)]);hi=Vector([max(v[i] for v in bounds) for i in range(3)])
 assert abs(max(hi-lo)-1.3)<.001,(path,lo,hi)
 assert all(abs(c)<.001 for c in (lo+hi)/2),(path,lo,hi)
 assert all(math.isfinite(c) for v in obj.data.vertices for c in v.normal),path
 images=[n.image for m in obj.data.materials for n in m.node_tree.nodes if n.type=='TEX_IMAGE'];assert images and all(im.packed_file for im in images),path
 cam.location=(2.4,-5,2.0);cam.rotation_euler=(-cam.location).to_track_quat('-Z','Y').to_euler()
 scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA';scene.render.filepath=str(ICONS/(path.stem+'.png'));bpy.ops.render.render(write_still=True)
 views=ART/'views';views.mkdir(exist_ok=True)
 for label,direction in [('front',(0,-5,.1)),('side',(5,0,.1)),('back',(0,5,.1)),('top',(0,0,5))]:
  cam.location=direction;cam.rotation_euler=(-cam.location).to_track_quat('-Z','Y').to_euler()
  scene.render.filepath=str(views/(path.stem+'_'+label+'.png'));bpy.ops.render.render(write_still=True)
 report.append({'name':path.stem,'size':list(hi-lo),'mesh_count':1,'material_count':1,'uv_layers':len(obj.data.uv_layers),'triangles':sum(len(p.vertices)-2 for p in obj.data.polygons),'embedded_texture':True})
 for o in objs:bpy.data.objects.remove(o,do_unlink=True)
# Reference assets under the same studio settings, fitted to the same visible size.
refs=ART/'references';refs.mkdir(exist_ok=True)
for filename in ['assets/models/items/nail.glb','assets/models/items/blueprint.glb','assets/models/powerups/magnet.glb']:
 before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(ROOT/filename));objs=[o for o in scene.objects if o not in before]
 bounds=[o.matrix_world@Vector(v) for o in objs if o.type=='MESH' for v in o.bound_box]
 lo=Vector([min(v[i] for v in bounds) for i in range(3)]);hi=Vector([max(v[i] for v in bounds) for i in range(3)]);center=(lo+hi)/2;factor=1.30/max(hi-lo)
 for o in objs:
  if o.type!='MESH':continue
  world=o.matrix_world.copy()
  for v in o.data.vertices:v.co=(world@v.co-center)*factor
  o.matrix_world.identity()
 cam.location=(2.4,-5,2.0);cam.rotation_euler=(-cam.location).to_track_quat('-Z','Y').to_euler()
 scene.render.filepath=str(refs/(Path(filename).stem+'.png'));bpy.ops.render.render(write_still=True)
 for o in objs:bpy.data.objects.remove(o,do_unlink=True)
(ART/'validation.json').write_text(json.dumps(report,indent=2))
print('Validated and rendered',len(report),'runtime models')

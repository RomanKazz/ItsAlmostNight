"""Increase mortar outer diameter by 25%, preserving the bore and current editable base."""
from pathlib import Path
from datetime import datetime
import math,shutil,json,bpy
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models';VIEWS=ART/'mortar_views'
scene=bpy.data.scenes['MORTAR'];bpy.context.window.scene=scene;bpy.context.view_layer.update()
assert not scene.get('thick_barrel_20261015'),'Thickness revision already applied'
backup=ROOT/'output/mortar_revisions'/datetime.now().strftime('%Y%m%d_%H%M%S_thickness');backup.mkdir(parents=True)
bpy.ops.wm.save_as_mainfile(filepath=str(backup/'mortar_before.blend'),copy=True,check_existing=False)
for p in ['assets/models/buildings/mortar.glb','art/models/mortar_manifest.json','art/models/mortar_handoff.md','art/models/mortar_preview.png','tools/create_mortar_model.py','tools/validate_mortar_model.py','tools/validate_mortar_loader.cpp']:
 shutil.copy2(ROOT/p,backup/Path(p).name)
base=scene.objects['base_mesh'];head=scene.objects['pitch_mesh'];mount=scene.objects['yaw_mesh'];pitch=scene.objects['pitch_pivot'];studio=bpy.data.collections['MORTAR_STUDIO']
for o in (head,mount):
 if o.data.users>1:o.data=o.data.copy()
def ids(o,name):
 g=o.vertex_groups[name].index
 return sorted(v.index for v in o.data.vertices if any(w.group==g for w in v.groups))
# Convert from the baked 45-degree neutral frame into the tube frame.
rotation=Matrix.Rotation(math.pi/4,3,'X');inverse=rotation.inverted()
for label,rings in [('mortar_barrel',6),('mortar_muzzle_rim',4),('mortar_breech_cap',2)]:
 for index in ids(head,label)[:rings*12]:
  v=head.data.vertices[index];co=inverse@v.co;co.x*=1.25;co.z*=1.25;v.co=rotation@co
for sign,side in [(-1,'left'),(1,'right')]:
 for label in ('mortar_yoke_'+side,'mortar_pivot_bolt_'+side):
  for i in ids(mount,label):
   v=mount.data.vertices[i];v.co.x+=sign*.065
   if v.co.z>.1:v.co.z+=.050
 # The short side axles move out with their bearing; the channel stays clear.
 for i in ids(head,'mortar_trunnion_'+side):head.data.vertices[i].co.x+=sign*.065
# Widen both platform layers to support the moved feet at every corner.
for label,factor in [('mortar_turntable',1.20),('mortar_yoke_platform',1.22)]:
 for i in ids(mount,label):
  v=mount.data.vertices[i];v.co.x*=factor;v.co.y*=factor
pitch.location.z+=.050
for o in (head,mount):o.data.update()
scene['thick_barrel_20261015']='Outer diameter +25%; bore preserved; matching supports widened'
bpy.context.view_layer.update()
col=scene.objects['COL_BOX_BODY'];col.hide_set(False);bpy.ops.object.select_all(action='DESELECT')
for o in scene.objects:
 if o.type in ('MESH','EMPTY'):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/mortar.glb'),use_selection=True,use_active_scene=True,
 use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,export_normals=True,
 export_materials='EXPORT',export_animations=False,export_extras=True)
col.hide_set(True)
def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
cam=scene.camera;data=cam.data
views=[('beauty',(-2.8,4,2.7),(0,0,.65),1.95),('front',(0,4,.66),(0,0,.66),1.85),('side',(-4,0,.66),(0,0,.66),1.85),('back',(0,-4,.66),(0,0,.66),1.85),('top',(0,.001,4),(0,0,0),1.55)]
for name,pos,target,scale in views:
 cam.location=pos;aim(cam,target);data.ortho_scale=scale;scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'mortar_preview.png');bpy.ops.render.render(write_still=True)
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'mortar.blend'),check_existing=False)
source=(ROOT/'tools/create_mortar_model.py').read_text()
exec(compile(source[source.index("comparison=bpy.data.scenes.new('Mortar_comparison')"):],'mortar_comparison','exec'))
print('MORTAR_THICKENED',str(backup))

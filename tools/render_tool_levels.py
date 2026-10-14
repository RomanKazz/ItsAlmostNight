"""Build editable tool upgrade studio and common-light acceptance previews.
Run in a fresh background Blender. Runtime GLBs remain the generator's output.
"""
from pathlib import Path
import bpy
from mathutils import Matrix,Vector

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/tool_levels'
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene;scene.name='IAN_TOOL_LEVELS';scene.unit_settings.system='METRIC'
model=bpy.data.collections.new('TOOLS_MODEL');studio=bpy.data.collections.new('TOOLS_STUDIO')
scene.collection.children.link(model);scene.collection.children.link(studio)
groups={}
for name in ('axe','pickaxe'):
 before=set(scene.objects)
 bpy.ops.import_scene.gltf(filepath=str(ROOT/f'assets/models/tools/{name}_levels.glb'))
 objects=[o for o in scene.objects if o not in before]
 for o in objects:
  for old in list(o.users_collection):old.objects.unlink(o)
  model.objects.link(o)
  if o.type=='MESH':
   o['tool']=name;o['level']=int(o.name.split('_L')[-1].split('.')[0]) if o.name.startswith('upgrade_') else 1
   if o['level']>1:
    for group,u in [('gold_trim',.65625),('orange_armor',.84375),('red_armor',.96875),('steel_edge',.78125)]:
     indices={loop.vertex_index for loop in o.data.loops if abs(o.data.uv_layers.active.data[loop.index].uv.x-u)<1e-5}
     if indices:o.vertex_groups.new(name=group).add(list(indices),1,'REPLACE')
 groups[name]=[o for o in objects if o.type=='MESH']
 # Preserve the authored hierarchy in the MODEL collection; preview clones are neutral.
 for o in objects:o.hide_render=True;o.hide_set(True)
for image in bpy.data.images:
 if image.source=='FILE':image.pack()
scene.render.engine='CYCLES';scene.cycles.samples=16
scene.render.resolution_x=640;scene.render.resolution_y=640;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='Standard'
scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.065,.085,.105,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.7

def move_studio(o):
 for old in list(o.users_collection):old.objects.unlink(o)
 studio.objects.link(o)
bpy.ops.object.camera_add();camera=bpy.context.object;camera.name='Neutral tool preview';move_studio(camera)
scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=.89
focus=Vector((0,0,.24))
def view(position):
 camera.location=position;camera.rotation_euler=(focus-camera.location).to_track_quat('-Z','Y').to_euler()
for name,pos,power,size in [('key',(2,-3,4),260,4),('fill',(-3,-1,2),180,3),('rim',(1,3,3),220,3)]:
 bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.name='Tool studio '+name;o.data.energy=power;o.data.size=size
 o.rotation_euler=(focus-o.location).to_track_quat('-Z','Y').to_euler();move_studio(o)
clones=[]
def clear():
 for o in clones:bpy.data.objects.remove(o,do_unlink=True)
 clones.clear()
def display(name,level,offset=0):
 for source in groups[name]:
  if source['level'] not in (1,level):continue
  o=source.copy();studio.objects.link(o);o.parent=None;o.animation_data_clear();o.matrix_world=Matrix.Identity(4);o.location.x=offset;o.hide_render=False;o.hide_set(False);clones.append(o)
for name in ('axe','pickaxe'):
 for level in range(1,9):
  clear();display(name,level);view((.85,-2,1.02))
  scene.render.filepath=str(ART/f'{name}_{level}.png');bpy.ops.render.render(write_still=True)
 for direction,pos in [('front',(0,-2,.24)),('side',(2,0,.24)),('back',(0,2,.24)),('top',(0,-.001,2))]:
  view(pos);scene.render.filepath=str(ART/f'{name}_{direction}.png');bpy.ops.render.render(write_still=True)
clear()
# Same source palette and light for tools and the neighboring gold defense family.
for name,level,x in [('axe',1,-1.75),('axe',4,-1.2),('axe',8,-.65),('pickaxe',1,.05),('pickaxe',4,.95),('pickaxe',8,1.85)]:display(name,level,x)
focus=Vector((.1,0,.25));view((.7,-4,1.5));camera.data.ortho_scale=4.4
scene.render.resolution_x=1800;scene.render.resolution_y=550
scene.render.filepath=str(ART/'comparison.png');bpy.ops.render.render(write_still=True)
clear()
for name,x in [('axe',-.85),('pickaxe',0)]:display(name,8,x)
for filename,x,scale in [('cannon.glb',.8,.5),('core_levels.glb',1.6,.3),('flamethrower.glb',2.5,.4)]:
 before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/models/buildings'/filename))
 bpy.context.view_layer.update()
 for o in [o for o in scene.objects if o not in before]:
  clones.append(o)
  if o.type!='MESH':continue
  matrix=o.matrix_world.copy();o.parent=None;o.matrix_world=Matrix.Scale(scale,4)@matrix;o.location.x+=x
  if o.name.startswith(('COL_','upgrade_')) or ('core_L' in o.name and 'core_L1' not in o.name):o.hide_render=True
focus=Vector((.8,0,.3));view((1,-5,2));camera.data.ortho_scale=4.6
scene.render.filepath=str(ART/'family.png');bpy.ops.render.render(write_still=True)
clear()
# Save editable sources, visible L8, original pickaxe rig/animations intact.
for name in groups:
 for o in groups[name]:
  o.hide_render=o['level'] not in (1,8);o.hide_set(o.hide_render)
# Studio clones are deliberately excluded from model export/asset validation.
scene.render.resolution_x=640;scene.render.resolution_y=640
focus=Vector((0,0,.24));view((.85,-2,1.02));camera.data.ortho_scale=.89
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'tool_levels.blend'))
# All requested tiers in one sheet, left to right L1-L8; axe above pickaxe.
import numpy as np
pixels=np.ones((640,2560,4),dtype=np.float32)
for row,name in enumerate(('pickaxe','axe')):
 for level in range(1,9):
  image=bpy.data.images.load(str(ART/f'{name}_{level}.png'),check_existing=False)
  image.scale(320,320)
  pixels[row*320:(row+1)*320,(level-1)*320:level*320,:]=np.asarray(image.pixels[:],dtype=np.float32).reshape(320,320,4)
  bpy.data.images.remove(image)
sheet=bpy.data.images.new('Tool levels overview',width=2560,height=640)
sheet.pixels.foreach_set(pixels.ravel());sheet.filepath_raw=str(ART/'all_levels.png');sheet.file_format='PNG';sheet.save()

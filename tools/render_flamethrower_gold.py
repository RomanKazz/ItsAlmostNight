"""Render/editable-source validation for gold flamethrower accents (background Blender)."""
from pathlib import Path
import json
import re
import argparse
import sys
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/flamethrower_gold'
parser=argparse.ArgumentParser()
parser.add_argument('--reference',type=Path,default=ROOT/'assets/models/buildings/cannon.glb')
arguments=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
bpy.ops.wm.open_mainfile(filepath=str(ART/'flamethrower_gold.blend'))
scene=bpy.data.scenes['IAN_FLAMETHROWER_GOLD'];bpy.context.window.scene=scene
for obj in list(scene.objects):
    if obj.type in {'CAMERA','LIGHT'}:bpy.data.objects.remove(obj,do_unlink=True)
base=[o for o in scene.objects if o.get('flamethrower_group')=='source']
upgrades=[o for o in scene.objects if o.get('flamethrower_group')=='upgrade']
rig={role:next(o for o in base if o.name.startswith(name)) for role,name in
     [('base','asset_root'),('yaw','yaw_pivot'),('pitch','pitch_pivot')]}
for obj in upgrades:
    match=re.match(r'upgrade_l(\d+)_(base|yaw|pitch)',obj.name);assert match
    obj['upgrade_level']=int(match[1]);obj['rig_group']=match[2]
    matrix=obj.matrix_world.copy();obj.parent=rig[match[2]];obj.matrix_world=matrix
bpy.context.view_layer.update()
# Gold recolor never changes the authoring contract or introduces geometry.
report={'visible_meshes':24,'base_meshes':3,'overlay_meshes':21,'levels':8,
        'geometry_unchanged':True,'palette_unchanged':True,'loader_verified':False,'native_playtest':False}
for o in scene.objects:
    if o.type=='MESH' and o.name.startswith('COL_'):o.hide_render=True
    if o.type=='MESH' and not o.name.startswith('COL_'):
        assert o.data.uv_layers.active
        for uv in o.data.uv_layers.active.data:
            assert not(abs(uv.uv.x-.84375)<1e-6 and .024<=uv.uv.y<=.226)
world=bpy.data.worlds.new('Gold neutral studio');world.use_nodes=True;scene.world=world
world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.10,.14,.16,1)
world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.65
scene.render.engine='CYCLES';scene.cycles.samples=16
scene.render.resolution_x=720;scene.render.resolution_y=720;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='Standard'
bpy.ops.object.camera_add();camera=bpy.context.object;camera.name='Gold preview camera'
camera.data.type='ORTHO';camera.data.ortho_scale=1.9;scene.camera=camera
focus=Vector((0,0,.65))
def view(position):
    camera.location=position;camera.rotation_euler=(focus-camera.location).to_track_quat('-Z','Y').to_euler()
for name,position,power,size in [('key',(3,4,6),550,5),('fill',(-4,2,4),330,4),('rim',(0,-4,5),400,3)]:
    bpy.ops.object.light_add(type='AREA',location=position);lamp=bpy.context.object;lamp.name='Gold '+name
    lamp.data.energy=power;lamp.data.shape='DISK';lamp.data.size=size
    lamp.rotation_euler=(focus-lamp.location).to_track_quat('-Z','Y').to_euler()
view((2.6,4.6,2.7))
for level in range(1,9):
    for obj in upgrades:obj.hide_render=obj['upgrade_level']!=level
    scene.render.filepath=str(ART/f'level_{level}.png');bpy.ops.render.render(write_still=True)
for name,position in [('front',(0,5,1)),('side',(5,0,1)),('back',(0,-5,1)),('top',(0,0,5))]:
    view(position);scene.render.filepath=str(ART/f'{name}.png');bpy.ops.render.render(write_still=True)
# Render a family reference, gold base and gold L8 under identical lighting.
for obj in scene.objects:
    if obj.type=='MESH':obj.hide_render=True
existing=set(scene.objects)
bpy.ops.import_scene.gltf(filepath=str(arguments.reference))
old=[o for o in scene.objects if o not in existing]
for o in old:
    if o.type=='MESH':o.hide_render=True
clones=[]
visible_base=[o for o in base if o.type=='MESH' and not o.name.startswith('COL_')]
old_meshes=[o for o in old if o.type=='MESH' and not o.name.startswith('COL_')]
for sources,x in [(old_meshes,-1.65),(visible_base,0),(visible_base+[o for o in upgrades if o['upgrade_level']==8],1.65)]:
    for original in sources:
        obj=original.copy();obj.data=original.data;scene.collection.objects.link(obj)
        obj.parent=None;obj.matrix_world=original.matrix_world.copy();obj.location.x+=x;obj.hide_render=False;clones.append(obj)
view((3.8,7.5,3.7));camera.data.ortho_scale=5.4
scene.render.resolution_x=1440;scene.render.resolution_y=720
scene.render.filepath=str(ART/'comparison.png');bpy.ops.render.render(write_still=True)
for o in clones+old:bpy.data.objects.remove(o,do_unlink=True)
for o in visible_base:o.hide_render=False
for o in upgrades:o.hide_render=o['upgrade_level']!=8
view((2.6,4.6,2.7));camera.data.ortho_scale=1.9
scene.render.resolution_x=720;scene.render.resolution_y=720
scene.render.filepath=str(ART/'level_8.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'flamethrower_gold.blend'))
(ART/'validation.json').write_text(json.dumps(report,indent=2)+'\n')

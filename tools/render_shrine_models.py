"""Normalize the editable source and render orthographic/family previews."""
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/shrines'
bpy.ops.wm.open_mainfile(filepath=str(ART/'shrines.blend'))
scene=bpy.data.scenes['IAN_SHRINES'];bpy.context.window.scene=scene
camera=scene.camera
for name,direction in [('front',(0,-7,0)),('side',(7,0,0)),('back',(0,7,0)),('top',(0,0,7)),('perspective',(3,-7,4))]:
    focus=Vector((0,0,1.3));camera.location=focus+Vector(direction)
    camera.rotation_euler=(focus-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(ART/(name+'.png'));bpy.ops.render.render(write_still=True)
for kind,x in [('player',-3),('building',-1)]:
    root=bpy.data.objects[kind+'_shrine_root'];root.location.x=x
    meshes=[o for o in scene.objects if o.type=='MESH' and o.parent==root]
    top=max(v.co.z for o in meshes for v in o.data.vertices)
    root.scale=(2.8/top,)*3
before=set(scene.objects)
for path,x,scale in [('assets/models/buildings/core.glb',1,1.76),('assets/models/buildings/flamethrower.glb',3,1)]:
    old=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(ROOT/path))
    for o in set(scene.objects)-old:
        if o.name.startswith(('COL_','core_upgrade_band')):o.hide_render=True
        if o.parent is None:o.location.x+=x;o.scale*=scale
camera.data.ortho_scale=10.0;focus=Vector((0,0,1.3));camera.location=focus+Vector((4,-10,4))
camera.rotation_euler=(focus-camera.location).to_track_quat('-Z','Y').to_euler();scene.render.resolution_x=1500;scene.render.resolution_y=700
scene.render.filepath=str(ART/'family-comparison.png');bpy.ops.render.render(write_still=True)
for o in set(scene.objects)-before:bpy.data.objects.remove(o,do_unlink=True)
for kind,x in [('player',-1.3),('building',1.3)]:
    root=bpy.data.objects[kind+'_shrine_root'];root.location.x=x;root.scale=(1,1,1)
scene.render.resolution_x=1200;scene.render.resolution_y=800;camera.data.ortho_scale=5.6
focus=Vector((0,0,1.1));camera.location=(3,-7,4);camera.rotation_euler=(focus-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath=str(ART/'preview.png');bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'shrines.blend'))
print('Saved packed editable shrine project and family/orthographic previews.')

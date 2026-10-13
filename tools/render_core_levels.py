"""Normalize isolated .blend and export neutral studio comparisons/views.

Run in a separate background Blender after create_core_levels.py (MCP).
"""
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/core_levels'
bpy.ops.wm.open_mainfile(filepath=str(ART/'core_levels.blend'))
scene=bpy.data.scenes['IAN_CORE_LEVELS'];bpy.context.window.scene=scene
camera=scene.camera
scene.render.filepath=str(ART/'levels.png')
bpy.ops.render.render(write_still=True)
saved=(camera.location.copy(),camera.rotation_euler.copy(),camera.data.ortho_scale)
for obj in scene.objects:
    if obj.name.startswith(('Preview_','L')):obj.hide_render=not obj.name.startswith('Preview_L8_')
focus=Vector((2.025,.875,.61));camera.data.ortho_scale=1.5
scene.render.resolution_x=640;scene.render.resolution_y=640
for name,direction in [('front',(0,-3,0)),('side',(3,0,0)),('back',(0,3,0)),('top',(0,0,3)),('perspective',(2,-3,2))]:
    camera.location=focus+Vector(direction)
    camera.rotation_euler=(focus-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(ART/(name+'.png'));bpy.ops.render.render(write_still=True)

# L1, L8 and two neighbors under identical neutral lighting, with the game's
# core scale baked into preview only. Neighbor gameplay models remain untouched.
for obj in scene.objects:
    if obj.type in ('MESH','FONT'):obj.hide_render=True
comparison=[]
for level,x in ((1,-2.7),(8,-.9)):
    center=Vector((((level-1)%4)*1.35-2.025,((level-1)//4)*1.75-.875,0))
    for source in list(scene.objects):
        if not source.name.startswith(f'Preview_L{level}_'):continue
        copy=source.copy();copy.data=source.data;scene.collection.objects.link(copy)
        copy.matrix_world=source.matrix_world.copy()
        copy.location=(copy.location-center)*1.76+Vector((x,0,0))
        copy.scale*=1.76;copy.hide_render=False;comparison.append(copy)
for path,x,exclude in [('assets/models/buildings/flamethrower.glb',.9,('COL_',)),('assets/models/weapons/crossbow.glb',2.7,('COL_','upgrade_'))]:
    before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(ROOT/path))
    imported=set(scene.objects)-before
    for obj in imported:
        if obj.type=='MESH':obj.hide_render=obj.name.startswith(exclude)
    for obj in imported:
        if obj.parent is None:obj.location.x+=x
    comparison.extend(imported)
camera.location=(4,-9,5);focus=Vector((0,0,.8))
camera.rotation_euler=(focus-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=7.4
scene.render.resolution_x=1500;scene.render.resolution_y=650
scene.render.filepath=str(ART/'family_comparison.png');bpy.ops.render.render(write_still=True)
for obj in comparison:bpy.data.objects.remove(obj,do_unlink=True)
for obj in scene.objects:
    if obj.name.startswith(('Preview_','L')):obj.hide_render=False
camera.location,camera.rotation_euler,camera.data.ortho_scale=saved
scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.filepath=str(ART/'levels.png')
# Ordinary editable project, not a scene library opening with the wrong scene.
bpy.context.view_layer.update()
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'core_levels.blend'))
print('Saved packed editable Blender project and neutral previews.')

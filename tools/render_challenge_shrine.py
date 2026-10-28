"""Render challenge-shrine views and normalize its isolated editable .blend."""
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/challenge_shrine'
bpy.ops.wm.open_mainfile(filepath=str(ART/'challenge_shrine.blend'))
scene=bpy.data.scenes['IAN_CHALLENGE_SHRINE_20261028'];bpy.context.window.scene=scene
root=bpy.data.objects['challenge_shrine_root'];camera=scene.camera
mesh=bpy.data.meshes.new('Challenge_Studio_Floor');mesh.from_pydata([(-200,-200,-.015),(200,-200,-.015),(200,200,-.015),(-200,200,-.015)],[],[(0,1,2,3)])
floor=bpy.data.objects.new('Challenge_Studio_Floor',mesh)
next(c for c in scene.collection.children if c.name.startswith('CHALLENGE_SHRINE_STUDIO')).objects.link(floor)
mat=bpy.data.materials.new('Challenge_Studio_Floor_Mat');mat.use_nodes=True
node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED');node.inputs['Base Color'].default_value=(.055,.075,.068,1);node.inputs['Roughness'].default_value=1
mesh.materials.append(mat)
def shot(name,eye,focus=(0,0,1.6),scale=4.15,resolution=(1100,1300)):
    focus=Vector(focus);camera.location=focus+Vector(eye)
    camera.rotation_euler=(focus-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=scale
    scene.render.resolution_x,scene.render.resolution_y=resolution
    scene.render.filepath=str(ART/(name+'.png'));bpy.ops.render.render(write_still=True)
for name,eye in [('front',(0,-8,0)),('back',(0,8,0)),('side',(8,0,0)),('top',(0,0,8))]:
    floor.hide_render=True;shot(name,eye)
floor.hide_render=False;shot('preview',(3,-8,3))
root.location.x=2.5;before=set(scene.objects)
for kind,x in [('player',-2.5),('building',0)]:
    old=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/models/world_landmarks'/(kind+'_shrine.glb')))
    for obj in set(scene.objects)-old:
        if obj.parent is None:obj.location.x=x
shot('family-comparison',(3,-10,3.8),focus=(0,0,1.5),scale=9.5,resolution=(1700,850))
for obj in set(scene.objects)-before:bpy.data.objects.remove(obj,do_unlink=True)
root.location.x=0
camera.data.ortho_scale=4.15;camera.location=(3,-8,4.6)
camera.rotation_euler=(Vector((0,0,1.6))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.resolution_x=1100;scene.render.resolution_y=1300;scene.render.filepath=str(ART/'preview.png')
scene['authoring_note']='Export MODEL collection only. Studio lights/floor never enter runtime GLB.'
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'challenge_shrine.blend'))
print('Saved editable packed challenge shrine and neutral/family views.')

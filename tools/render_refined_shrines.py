"""Render neutral views and asset-family comparison, then save editable source."""
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/shrines/rework'
bpy.ops.wm.open_mainfile(filepath=str(ART / 'shrines.blend'))
scene = bpy.data.scenes['IAN_SHRINES_REWORK_20261028']
bpy.context.window.scene = scene
camera = scene.camera
roots = [bpy.data.objects['woodland_' + kind + '_shrine_root'] for kind in ('player', 'building')]

# A neutral studio floor gives scale and contact without baking shadows into assets.
floor_mesh = bpy.data.meshes.new('Shrine_Studio_Floor')
floor_mesh.from_pydata([(-200,-200,-.015),(200,-200,-.015),(200,200,-.015),(-200,200,-.015)],[],[(0,1,2,3)])
floor = bpy.data.objects.new('Shrine_Studio_Floor', floor_mesh)
next(c for c in scene.collection.children if c.name.startswith('SHRINES_REWORK_STUDIO')).objects.link(floor)
floor_mat = bpy.data.materials.new('Shrine_Studio_Floor_Material');floor_mat.use_nodes=True
node = next(n for n in floor_mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
node.inputs['Base Color'].default_value=(.055,.075,.068,1)
node.inputs['Roughness'].default_value=1
floor_mesh.materials.append(floor_mat)

def shot(name, eye, focus=(0,0,1.4), scale=5.6, resolution=(1400,1000)):
    focus=Vector(focus);camera.location=focus+Vector(eye)
    camera.rotation_euler=(focus-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=scale
    scene.render.resolution_x,scene.render.resolution_y=resolution
    scene.render.filepath=str(ART/(name+'.png'))
    bpy.ops.render.render(write_still=True)

for name,eye in [('front',(0,-8,0)),('side',(8,0,0)),('back',(0,8,0)),
                 ('top',(0,0,8)),('perspective',(3,-8,3))]:
    floor.hide_render=name in ('front','side','back','top')
    shot(name,eye)
floor.hide_render=False
for root in roots:
    other=roots[1] if root==roots[0] else roots[0]
    for obj in [other]+list(other.children):obj.hide_render=True
    shot('player-detail' if root==roots[0] else 'building-detail',(3,-8,2.6),
         focus=(root.location.x,0,1.4),scale=3.8,resolution=(1100,1100))
    for obj in [other]+list(other.children):obj.hide_render=False

original_locations=[root.location.copy() for root in roots]
for root,x in zip(roots,(-3.3,-1.1)):root.location.x=x
before=set(scene.objects)
for path,x,scale in [('assets/models/buildings/core.glb',1.1,1.76),
                    ('assets/models/buildings/gun_turret.glb',3.1,1)]:
    old=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(ROOT/path))
    for obj in set(scene.objects)-old:
        if obj.parent is None:obj.location.x+=x;obj.scale*=scale
        if obj.name.startswith(('COL_','core_upgrade_band','upgrade_')):obj.hide_render=True
shot('family-comparison',(3,-10,3.5),focus=(0,0,1.3),scale=10,resolution=(1700,800))
for obj in set(scene.objects)-before:bpy.data.objects.remove(obj,do_unlink=True)

# Before/after keeps both generations under exactly the same studio lighting.
for root,x in zip(roots,(1.1,3.3)):root.location.x=x
before=set(scene.objects)
legacy_source=ROOT/'art/models/shrines/shrines.blend'
with bpy.data.libraries.load(str(legacy_source),link=False) as (source,destination):
    destination.objects=[name for name in source.objects
                         if name.startswith(('player_shrine_','building_shrine_'))]
for obj in destination.objects:
    if obj:scene.collection.objects.link(obj)
for kind,x in [('player',-3.3),('building',-1.1)]:
    root=next(o for o in destination.objects if o and o.name.startswith(kind+'_shrine_root'))
    meshes=[o for o in destination.objects if o and o.type=='MESH' and o.parent==root]
    top=max(v.co.z for o in meshes for v in o.data.vertices)
    root.location=(x,0,0);root.scale=(2.8/top,)*3
shot('before-after',(2,-10,3),focus=(0,0,1.4),scale=10,resolution=(1800,900))
for obj in set(scene.objects)-before:bpy.data.objects.remove(obj,do_unlink=True)
for root,location in zip(roots,original_locations):root.location=location
shot('preview',(3,-8,3))
scene['authoring_note']='Model and studio collections are separate. Export MODEL only. Runtime GLBs have two meshes / one packed palette material.'
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'shrines.blend'))
print('Saved ordinary editable .blend with packed colormap and neutral previews.')

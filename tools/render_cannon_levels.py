"""Isolated editable cannon rig, neutral previews and clean GLB validation.

Blender --factory-startup --background --python-exit-code 1 --python tools/render_cannon_levels.py
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import sys
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/cannon_levels'
ART.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--asset', type=Path, default=ROOT / 'assets/models/buildings/cannon.glb')
parser.add_argument('--baseline', action='store_true')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.name = 'IAN_CANNON_LEVELS'
scene.unit_settings.system = 'METRIC'
bpy.ops.import_scene.gltf(filepath=str(args.asset))
source = list(scene.objects)
collections = {}
for name in ('CANNON_MODEL','CANNON_COLLISION','CANNON_STUDIO'):
    collection = bpy.data.collections.new(name)
    scene.collection.children.link(collection)
    collections[name] = collection
for obj in source:
    collection = collections['CANNON_COLLISION' if obj.name.startswith('COL_') else 'CANNON_MODEL']
    for old in list(obj.users_collection): old.objects.unlink(obj)
    collection.objects.link(obj)
overlays = [o for o in source if o.name.startswith('upgrade_cannon_')]
barrel = next(o for o in source if o.type == 'MESH' and o.name.startswith('barrel'))
carriage = next(o for o in source if o.type == 'MESH' and o.name.startswith('weapon-cannon'))
# Authored pivot matches the current runtime (legacy Kenney contract).
pitch = barrel.parent
pitch.rotation_mode = 'XYZ'
base = [o for o in source if o.type == 'MESH' and o not in overlays and not o.name.startswith('COL_')]
for obj in overlays:
    obj['level'] = int(obj.name.split('_')[2][1:])
    obj['motion'] = 'pitch' if obj.name.endswith('_barrel') else 'yaw'
    matrix = obj.matrix_world.copy()
    obj.parent = pitch if obj['motion'] == 'pitch' else carriage
    obj.matrix_world = matrix
    # Named palette groups retain useful editing selections within the two-draw rig.
    for name, u in [('gold_trim',.65625),('orange_armor',.84375),('red_armor',.96875),('dark_metal',.59375)]:
        vertices = set()
        for loop in obj.data.loops:
            if abs(obj.data.uv_layers.active.data[loop.index].uv.x-u)<1e-5:
                vertices.add(loop.vertex_index)
        if vertices:
            group = obj.vertex_groups.new(name=name)
            group.add(list(vertices),1,'REPLACE')
for obj in source:
    if obj.name.startswith('COL_'): obj.hide_render = True
bpy.context.view_layer.update()
for obj in base + overlays:
    if not args.baseline: assert obj.data.uv_layers.active, obj.name
    assert all(math.isfinite(v) for vertex in obj.data.vertices for v in vertex.co)
    assert all(math.isfinite(v) for polygon in obj.data.polygons for v in polygon.normal)
for image in bpy.data.images:
    if image.source == 'FILE': image.pack()
world = bpy.data.worlds.new('Cannon neutral studio')
world.use_nodes = True
world.node_tree.nodes.get('Background').inputs['Color'].default_value = (.10,.14,.16,1)
world.node_tree.nodes.get('Background').inputs['Strength'].default_value = .65
scene.world = world
scene.render.engine = 'CYCLES'
scene.cycles.samples = 12
scene.render.resolution_x = 640
scene.render.resolution_y = 640
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'Standard'
bpy.ops.object.camera_add()
camera = bpy.context.object
camera.name = 'Cannon studio camera'
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 1.85
scene.camera = camera
focus = Vector((0,0,.56))
def view(position):
    camera.location = position
    camera.rotation_euler = (focus-camera.location).to_track_quat('-Z','Y').to_euler()
for name, position, power, size in [('key',(3,4,6),550,5),('fill',(-4,2,4),330,4),('rim',(0,-4,5),400,3)]:
    bpy.ops.object.light_add(type='AREA', location=position)
    obj = bpy.context.object
    obj.name = 'Cannon studio '+name
    obj.data.energy = power
    obj.data.shape = 'DISK'
    obj.data.size = size
    obj.rotation_euler = (focus-obj.location).to_track_quat('-Z','Y').to_euler()
view((2.6,4.6,2.5))
for level in (1,8) if args.baseline else range(1,9):
    for obj in overlays: obj.hide_render = obj['level'] != level
    scene.render.filepath = str(ART/f'{"before_" if args.baseline else ""}level_{level}.png')
    bpy.ops.render.render(write_still=True)
for name, position in [('front',(0,5,.8)),('side',(5,0,.8)),('back',(0,-5,.8)),('top',(0,0,5))]:
    view(position)
    scene.render.filepath = str(ART/f'{"before_" if args.baseline else ""}{name}.png')
    bpy.ops.render.render(write_still=True)
if args.baseline: sys.exit(0)
view((2.6,4.6,2.5))
for degree in (-15,55):
    pitch.rotation_euler.x = math.radians(degree)
    bpy.context.view_layer.update()
    scene.render.filepath = str(ART/f'pitch_{degree}.png')
    bpy.ops.render.render(write_still=True)
pitch.rotation_euler.x = 0
bpy.context.view_layer.update()
# Common light and scale, without AO/bloom: L1, L4, L8 + the gold flamethrower.
for obj in base + overlays: obj.hide_render = True
clones = []
for level, x in [(1,-2.1),(4,-.7),(8,.7)]:
    for original in base + [o for o in overlays if o['level']==level]:
        obj = original.copy()
        scene.collection.objects.link(obj)
        obj.parent = None
        obj.matrix_world = original.matrix_world.copy()
        obj.location.x += x
        obj.hide_render = False
        clones.append(obj)
previous = set(scene.objects)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/models/buildings/flamethrower.glb'))
reference = [o for o in scene.objects if o not in previous]
for o in reference:
    matrix = o.matrix_world.copy()
    o.parent = None
    o.matrix_world = matrix
    o.location.x += 2.1
    if o.name.startswith('COL_'): o.hide_render = True
view((2,10,4.1))
camera.data.ortho_scale = 6.4
scene.render.resolution_x = 1600
scene.render.resolution_y = 640
scene.render.filepath = str(ART/'comparison.png')
bpy.ops.render.render(write_still=True)
for obj in clones + reference: bpy.data.objects.remove(obj, do_unlink=True)
for obj in base: obj.hide_render = False
for obj in overlays:
    obj.hide_render = obj['level'] != 8
    obj.hide_set(obj.hide_render)
for obj in source:
    if obj.name.startswith('COL_'):
        obj.display_type = 'WIRE'
        obj.hide_set(True)
for obj in list(scene.objects):
    if obj.type in {'LIGHT','CAMERA'}:
        for old in list(obj.users_collection): old.objects.unlink(obj)
        collections['CANNON_STUDIO'].objects.link(obj)
view((2.6,4.6,2.5))
camera.data.ortho_scale = 1.85
scene.render.resolution_x = 640
scene.render.resolution_y = 640
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'cannon_levels.blend'))
print('Saved editable rig and eight neutral previews:', ART)

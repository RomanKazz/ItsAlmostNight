"""Author a symmetrical Kenney-style Tesla; run with Blender's Python.

blender --background --factory-startup --python tools/create_tesla_model.py
Keep original references, palette UVs and editable model parts in the .blend.
"""
import argparse
import collections
import json
import math
from pathlib import Path
import shutil
import sys

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--kit', type=Path, default=ROOT / 'art/references/kenney_tower_defense')
parser.add_argument('--previews', type=Path, default=ROOT / 'output/tesla')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
args.previews.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
model = bpy.data.collections.new('TESLA_MODEL')
bpy.context.scene.collection.children.link(model)
studio = bpy.data.collections.new('PREVIEW_STUDIO')
bpy.context.scene.collection.children.link(studio)


def move_to(obj, collection):
    for existing in list(obj.users_collection):
        existing.objects.unlink(obj)
    collection.objects.link(obj)
    return obj


bpy.ops.import_scene.gltf(filepath=str(args.kit / 'weapon-cannon.glb'))
source = bpy.data.objects['weapon-cannon']
palette = source.data.materials[0]
palette.name = 'colormap'
texture = next(node for node in palette.node_tree.nodes if node.type == 'TEX_IMAGE')
image = texture.image
image.name = 'colormap.png'
image.filepath = str((ROOT / 'assets/models/buildings/Textures/colormap.png').resolve())
image.pack()
pixels = list(image.pixels)


def palette_uv(color):
    """Pick an interior pixel in the original map, never a new RGB material."""
    width, height = image.size
    target = [channel / 255 for channel in color]
    best = None
    for y in range(2, height - 2):
        for x in range(2, width - 2):
            i = (y * width + x) * 4
            sample = pixels[i:i + 3]
            if max(sample) < .05:
                continue
            error = sum((sample[c] - target[c]) ** 2 for c in range(3))
            # Avoid a discontinuity at swatch borders under bilinear sampling.
            neighbors = [(y * width + x - 1) * 4, (y * width + x + 1) * 4,
                         ((y - 1) * width + x) * 4, ((y + 1) * width + x) * 4]
            error += sum(sum((pixels[n + c] - sample[c]) ** 2 for c in range(3))
                         for n in neighbors) * .5
            if best is None or error < best[0]:
                best = (error, (x + .5) / width, (y + .5) / height)
    return best[1:]


ORANGE = palette_uv((255, 121, 43))
ORANGE_LIGHT = palette_uv((255, 146, 60))
ORANGE_DARK = palette_uv((211, 81, 28))
STEEL = palette_uv((139, 165, 196))
DARK_STEEL = palette_uv((64, 87, 112))
CYAN = palette_uv((130, 242, 255))


def assign_uv(obj, uv, material=palette):
    obj.data.materials.clear()
    obj.data.materials.append(material)
    layer = obj.data.uv_layers.active or obj.data.uv_layers.new(name='UVMap')
    for loop in layer.data:
        loop.uv = uv


def new_mesh(name, vertices, faces, uv):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    model.objects.link(obj)
    assign_uv(obj, uv)
    return obj


# Recover disconnected authored parts without guessing triangle ranges. The
# reference GLB duplicates UV/normal seam vertices, hence the position weld.
mesh = source.data
coords = {v.index: tuple(round(c, 5) for c in v.co) for v in mesh.vertices}
adjacent = collections.defaultdict(set)
for face in mesh.polygons:
    points = [coords[i] for i in face.vertices]
    for point in points:
        adjacent[point].update(points)
remaining = set(adjacent)
groups = []
while remaining:
    todo = [min(remaining)]
    group = set()
    while todo:
        point = todo.pop()
        if point in group:
            continue
        group.add(point)
        todo.extend(adjacent[point] - group)
    remaining -= group
    groups.append(group)


def bounds(group):
    return [min(v[i] for v in group) for i in range(3)], [max(v[i] for v in group) for i in range(3)]


def copy_component(name, group, rotation=0, uv=None):
    polygons = [p for p in mesh.polygons if coords[p.vertices[0]] in group]
    used = sorted({v for p in polygons for v in p.vertices})
    indices = {v: i for i, v in enumerate(used)}
    obj = new_mesh(name, [mesh.vertices[v].co[:] for v in used],
                   [[indices[v] for v in p.vertices] for p in polygons], ORANGE)
    if uv is None:
        layer = obj.data.uv_layers.active
        source_uv = mesh.uv_layers.active
        for output, original in zip(obj.data.polygons, polygons):
            for a, b in zip(output.loop_indices, original.loop_indices):
                layer.data[a].uv = source_uv.data[b].uv
    else:
        assign_uv(obj, uv)
    obj.rotation_euler.z = rotation
    return obj


base_group = next(g for g in groups if bounds(g)[0][2] == 0)
brace_group = next(g for g in groups if bounds(g)[0][0] > 0 and bounds(g)[1][2] > .43)
bolt_group = next(g for g in groups if bounds(g)[0][0] > .20 and bounds(g)[1][2] < .34)
base = copy_component('tesla_base', base_group, uv=ORANGE)
for i in range(4):
    angle = i * math.tau / 4
    copy_component(f'tesla_brace_{i + 1}', brace_group, angle, uv=STEEL)
    copy_component(f'tesla_bolt_{i + 1}', bolt_group, angle, uv=DARK_STEEL)


def lathe(name, profile, uv, segments=16, smooth=True):
    vertices = [(r * math.cos(i * math.tau / segments),
                 r * math.sin(i * math.tau / segments), z)
                for r, z in profile for i in range(segments)]
    faces = []
    for ring in range(len(profile) - 1):
        for i in range(segments):
            a = ring * segments + i
            b = ring * segments + (i + 1) % segments
            faces.append((a, b, b + segments, a + segments))
    faces += [tuple(reversed(range(segments))),
              tuple((len(profile) - 1) * segments + i for i in range(segments))]
    obj = new_mesh(name, vertices, faces, uv)
    for polygon in obj.data.polygons[:-2]:
        polygon.use_smooth = smooth
    obj.data.set_sharp_from_angle(angle=math.radians(48))
    return obj


# Adapt the reference plinth into one continuous round foot, removing its
# recessed underside so all four supports meet a clean, solid orange base.
bpy.data.objects.remove(base, do_unlink=True)
base = lathe('tesla_base', [(.281, 0), (.30, .014), (.30, .133), (.281, .15)],
             ORANGE, segments=12)
lathe('tesla_column', [(.142, .145), (.142, .175), (.117, .197),
    (.117, .332), (.139, .354), (.139, .418), (.12, .435)], ORANGE)
lathe('tesla_lower_collar', [(.117, .415), (.135, .428), (.135, .457),
    (.116, .467)], ORANGE_LIGHT)
lathe('tesla_insulator', [(.056, .454), (.056, .653)], DARK_STEEL)
for i in range(3):
    z = .474 + i * .054
    lathe(f'tesla_coil_rib_{i + 1}', [(.057, z), (.087, z + .007),
        (.097, z + .019), (.087, z + .031), (.057, z + .038)], STEEL)
lathe('tesla_upper_collar', [(.110, .633), (.125, .644), (.125, .715),
    (.110, .726)], ORANGE_LIGHT)
lathe('tesla_emitter_seat', [(.070, .718), (.074, .728), (.074, .745),
    (.066, .754)], ORANGE_DARK)
lathe('tesla_spire', [(.073, .742), (.081, .765), (.019, 1.005),
    (.012, 1.028)], STEEL, segments=8, smooth=False)
for i in range(4):
    z = .745
    angle = i * math.tau / 4
    contact = lathe(f'tesla_contact_{i + 1}', [(.013, z), (.013, z + .017)], ORANGE_LIGHT, segments=8)
    contact.location.x, contact.location.y = .072 * math.cos(angle), .072 * math.sin(angle)

energy = palette.copy()
energy.name = 'colormap_energy'
shader = next(n for n in energy.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
energy_texture = next(n for n in energy.node_tree.nodes if n.type == 'TEX_IMAGE')
energy.node_tree.links.new(energy_texture.outputs['Color'], shader.inputs['Emission Color'])
shader.inputs['Emission Strength'].default_value = 2.5
bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=8, radius=.040, location=(0, 0, 1.063))
core = move_to(bpy.context.object, model)
core.name = 'tesla_energy_core'
core.data.name = core.name
assign_uv(core, CYAN, energy)

for name, position in [('socket_base', (0, 0, 0)), ('socket_muzzle', (0, 0, 1.063)),
                        ('socket_effect_energy', (0, 0, 1.063))]:
    empty = bpy.data.objects.new(name, None)
    model.objects.link(empty)
    empty.location = position
    empty.empty_display_type = 'PLAIN_AXES'
    empty.empty_display_size = .045

# Imports are references only; the delivered .blend contains just Tesla and
# a separate studio collection. Original GLBs remain in art/references.
for obj in list(bpy.data.objects):
    if obj.name not in model.objects:
        bpy.data.objects.remove(obj, do_unlink=True)

root = bpy.data.objects.new('weapon_tesla', None)
model.objects.link(root)
for obj in list(model.objects):
    if obj != root:
        obj.parent = root
bpy.ops.object.select_all(action='DESELECT')
for obj in model.objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = base
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
destination = ROOT / 'assets/models/buildings/tesla.glb'
bpy.ops.export_scene.gltf(filepath=str(destination), export_format='GLB',
    use_selection=True, export_yup=True, export_texcoords=True,
    export_normals=True, export_materials='EXPORT', export_animations=False)

# Preview-only lights, stage and cameras are deliberately excluded from GLB.
bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -.001))
ground = move_to(bpy.context.object, studio)
ground.name = 'preview_ground'
ground_material = bpy.data.materials.new('preview_ground')
ground_material.diffuse_color = (.028, .044, .06, 1)
ground_material.use_nodes = True
ground_shader = next(n for n in ground_material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
ground_shader.inputs['Base Color'].default_value = (.028, .044, .06, 1)
ground_shader.inputs['Roughness'].default_value = .85
ground.data.materials.append(ground_material)


def aim(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


for name, position, power, size in [('key', (2, -3, 4), 120, 3),
    ('fill', (-2, -1, 2), 90, 3), ('rim', (0, 2, 3), 140, 2)]:
    data = bpy.data.lights.new(name, 'AREA')
    data.energy, data.shape, data.size = power, 'DISK', size
    obj = bpy.data.objects.new(name, data)
    studio.objects.link(obj)
    obj.location = position
    aim(obj, (0, 0, .45))
bpy.ops.object.camera_add(location=(1.6, -2.6, 1.7))
camera = move_to(bpy.context.object, studio)
camera.name = 'preview_camera'
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 1.5
scene = bpy.context.scene
scene.camera = camera
scene.render.engine = 'CYCLES'
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.render.resolution_x, scene.render.resolution_y = 800, 1000
scene.render.resolution_percentage = 100
scene.view_settings.view_transform = 'Standard'
scene.view_settings.exposure = -.2
scene.world.use_nodes = True
background = next(n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND')
background.inputs['Color'].default_value = (.20, .25, .30, 1)
background.inputs['Strength'].default_value = .45
scene.render.image_settings.file_format = 'PNG'
compositor = bpy.data.node_groups.new('TeslaPreview', 'CompositorNodeTree')
compositor.interface.new_socket(name='Image', in_out='OUTPUT', socket_type='NodeSocketColor')
scene.compositing_node_group = compositor
nodes = compositor.nodes
layers = nodes.new('CompositorNodeRLayers')
glow = nodes.new('CompositorNodeGlare')
glow.inputs['Type'].default_value = 'Fog Glow'
glow.inputs['Quality'].default_value = 'High'
composite = nodes.new('NodeGroupOutput')
compositor.links.new(layers.outputs['Image'], glow.inputs['Image'])
compositor.links.new(glow.outputs['Image'], composite.inputs['Image'])
bpy.context.preferences.filepaths.save_version = 0
views = [('beauty', (1.6, -2.6, 1.7), (0, 0, .5), 1.5),
         ('front', (0, -3, .55), (0, 0, .55), 1.4),
         ('side', (3, 0, .55), (0, 0, .55), 1.4),
         ('back', (0, 3, .55), (0, 0, .55), 1.4),
         ('top', (0, -.001, 3), (0, 0, 0), .84)]
for name, position, target, scale in views:
    camera.location = position
    aim(camera, target)
    camera.data.ortho_scale = scale
    scene.render.filepath = str(args.previews / f'tesla_{name}.png')
    bpy.ops.render.render(write_still=True)
camera.location = views[0][1]
aim(camera, views[0][2])
camera.data.ortho_scale = 1.5
bpy.ops.object.select_all(action='DESELECT')
for obj in model.objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = base
for area in bpy.context.screen.areas:
    if area.type == 'VIEW_3D':
        area.spaces.active.region_3d.view_location = (0, 0, .52)
        area.spaces.active.region_3d.view_distance = 2
        area.spaces.active.shading.type = 'MATERIAL'
studio.hide_viewport = True
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / 'art/models/tesla.blend'))
meshes = [obj for obj in model.objects if obj.type == 'MESH']
report = {'objects': len(meshes), 'triangles': sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in meshes),
          'palette_uv': {'orange': ORANGE, 'steel': STEEL, 'cyan': CYAN},
          'height_m': 1.103, 'width_m': .6}
(args.previews / 'model_report.json').write_text(json.dumps(report, indent=2))
shutil.copyfile(args.previews / 'tesla_beauty.png', ROOT / 'art/models/tesla_preview.png')
view_directory = ROOT / 'art/models/tesla_views'
view_directory.mkdir(exist_ok=True)
for view in ('front', 'side', 'back', 'top'):
    shutil.copyfile(args.previews / f'tesla_{view}.png', view_directory / f'{view}.png')
print('TESLA_REPORT', json.dumps(report))

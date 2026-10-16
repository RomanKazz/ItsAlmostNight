"""Eight complete laser variants with the original palette, rig and muzzle.

Call build() through Blender MCP or run in a fresh background bpy process.
Render from the saved source with --render. Original laser files are preserved.
"""
from pathlib import Path
import hashlib
import importlib.util
import math
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/laser_levels'
OUT = ROOT / 'assets/models/buildings/laser_levels'
BASE = ROOT / 'assets/models/buildings/laser.glb'
SOURCE = ROOT / 'art/models/laser.blend'
ART.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('laser_level_helpers', ROOT / 'tools/create_wind_blower_levels.py')
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
GOLD, RED, ORANGE, DARK = common.GOLD, common.RED, common.ORANGE, common.DARK
CYAN = (.03125, .105, .205)
DESCRIPTIONS = (
    'Original laser', 'Gold lens rim, reactor frame and foot belt',
    'Gold braces and yoke with raised orange inlays', 'Four focusing rails and reinforced emitter collar',
    'Twin capacitor banks with cyan terminals', 'Extended capacitor banks and cooling fins',
    'Rear reactor cage and double pedestal belt', 'Master reactor collar and reinforced emitter cage',
)


def upgrade(level, objects, collection):
    base, yaw, head, energy = [objects[k] for k in ('base_mesh', 'yaw_mesh', 'pitch_mesh', 'energy_mesh')]
    pieces = {role: common.geometry.Geometry() for role in ('base', 'yaw', 'head', 'energy')}
    if level >= 2:
        common.recolor(head, 'laser_emitter_rim', GOLD)
        common.recolor(head, 'laser_reactor_frame', GOLD)
        pieces['base'].ring('gold_foot_belt', .176, .220, .588, .625, GOLD)
    if level >= 3:
        common.recolor(base, 'laser_base_brace_', GOLD)
        common.recolor(yaw, 'laser_yoke_', GOLD)
        common.recolor(head, 'laser_carry_handle', GOLD)
        pieces['yaw'].ring('gold_mount_belt', .682, .708, .427, .451, GOLD)
        for side in (-1, 1):
            lo, hi = sorted((side * .283, side * .300))
            common.side_panel(pieces['yaw'], 'orange_yoke_inlay_%d' % side, lo, hi,
                [(-.239, .735), (-.101, .735), (-.114, .914), (-.226, .914)], ORANGE)
    if level >= 4:
        # Rails seat into the emitter shell, leaving the central lens unobstructed.
        common.axis_lathe(pieces['head'], 'gold_emitter_collar',
            [(.175, .305), (.195, .305), (.195, .334), (.175, .334)], GOLD,
            (0, 0, 1.020), 8, hollow=True)
        for i in range(4):
            angle = math.pi / 4 + i * math.pi / 2
            start = len(pieces['head'].verts)
            common.box(pieces['head'], 'focusing_rail_%d' % i, (0, .282, .177), (.045, .160, .036), GOLD)
            for j in range(start, len(pieces['head'].verts)):
                x, y, z = pieces['head'].verts[j]
                pieces['head'].verts[j] = (x * math.cos(angle) + z * math.sin(angle), y,
                                           1.020 - x * math.sin(angle) + z * math.cos(angle))
    if level >= 5:
        rear = -.460 if level >= 6 else -.310
        for side in (-1, 1):
            x, z = side * .315, 1.230
            common.box(pieces['head'], 'capacitor_saddle_%d' % side, (side * .245, -.180, 1.187),
                       (.145, .160, .068), DARK)
            common.axis_lathe(pieces['head'], 'capacitor_bank_%d' % side,
                [(.056, rear), (.070, rear + .025), (.070, -.040), (.058, -.020)], RED, (x, 0, z), 8)
            for i, y in enumerate((rear + .030, -.070)):
                common.axis_lathe(pieces['head'], 'capacitor_band_%d_%d' % (side, i),
                    [(.068, y), (.079, y), (.079, y + .025), (.068, y + .025)], GOLD,
                    (x, 0, z), 8, hollow=True)
            common.axis_lathe(pieces['head'], 'capacitor_front_seat_%d' % side,
                [(.060, -.032), (.069, -.020), (.069, -.004)], GOLD, (x, 0, z), 8)
            common.axis_lathe(pieces['energy'], 'capacitor_terminal_%d' % side,
                [(.042, -.004), (.042, .005), (.031, .014)], CYAN, (x, 0, z), 8)
    if level >= 6:
        for i, y in enumerate((.020, .082, .144)):
            common.axis_lathe(pieces['head'], 'gold_cooling_fin_%d' % i,
                [(.132, y), (.153, y), (.153, y + .014), (.132, y + .014)], GOLD,
                (0, 0, 1.020), 10, hollow=True)
        pieces['base'].ring('gold_pedestal_collar', .518, .549, .222, .274, GOLD)
    if level >= 7:
        pieces['base'].ring('lower_master_belt', .096, .132, .588, .638, GOLD)
        # Rear cage follows pitch with the reactor, and stays behind the yoke.
        for i in range(4):
            angle = math.pi / 4 + i * math.pi / 2
            start = len(pieces['head'].verts)
            common.box(pieces['head'], 'reactor_cage_rail_%d' % i,
                       (0, -.330, .232), (.038, .180, .033), GOLD)
            for j in range(start, len(pieces['head'].verts)):
                x, y, z = pieces['head'].verts[j]
                pieces['head'].verts[j] = (x * math.cos(angle) + z * math.sin(angle), y,
                                           1.020 - x * math.sin(angle) + z * math.cos(angle))
    if level >= 8:
        pieces['base'].ring('master_red_foot_belt', .227, .266, .589, .629, RED)
        # An open-bottom guard clears the turntable at maximum upward pitch.
        profile = ((.225, -.436), (.269, -.436), (.269, -.407), (.225, -.407))
        vertices = [(r * math.sin(a), y, 1.020 + r * math.cos(a))
                    for i in range(9) for a in [math.radians(-120 + i * 30)] for r, y in profile]
        faces = [(4 * i + j, 4 * i + (j + 1) % 4,
                  4 * (i + 1) + (j + 1) % 4, 4 * (i + 1) + j)
                 for i in range(8) for j in range(4)]
        faces += [(3, 2, 1, 0), (32, 33, 34, 35)]
        pieces['head'].add('master_reactor_guard', vertices, faces, GOLD)
        common.axis_lathe(pieces['head'], 'master_emitter_collar',
            [(.169, .360), (.187, .360), (.187, .385), (.169, .385)], GOLD,
            (0, 0, 1.020), 8, hollow=True)
        for side in (-1, 1):
            lo, hi = sorted((side * .3005, side * .311))
            for i in range(3):
                z = .756 + i * .045
                common.side_panel(pieces['yaw'], 'master_rank_%d_%d' % (side, i), lo, hi,
                    [(-.215, z), (-.125, z), (-.125, z + .017), (-.215, z + .017)], GOLD)
    for role, obj in (('base', base), ('yaw', yaw), ('head', head), ('energy', energy)):
        common.append_geometry(obj, pieces[role], collection, obj.data.materials[0])
        obj.data.update()
    objects['socket_ui_top'].location.z = 1.550
    if level > 1:
        for v in objects['COL_BOX_BODY'].data.vertices:
            v.co.x = math.copysign(.700, v.co.x)
            v.co.y = math.copysign(.700, v.co.y)
            v.co.z = 0 if v.co.z < .01 else 1.500


def build():
    assert not bpy.data.scenes.get('LASER_L01'), 'Preserve manual level edits before rebuilding.'
    original_hash = hashlib.sha256(BASE.read_bytes()).hexdigest()
    if bpy.data.scenes.get('LASER'):
        original = bpy.data.scenes['LASER']
    else:
        with bpy.data.libraries.load(str(SOURCE), link=False) as (available, loaded):
            loaded.scenes = ['LASER']
        original = loaded.scenes[0]
    sources = [o for o in original.objects if o.type not in ('LIGHT', 'CAMERA')]
    scenes = []
    for level in range(1, 9):
        scene = bpy.data.scenes.new('LASER_L%02d' % level)
        bpy.context.window.scene = scene
        scene.unit_settings.system, scene.unit_settings.scale_length = 'METRIC', 1
        model = bpy.data.collections.new(scene.name + '_MODEL')
        collision = bpy.data.collections.new(scene.name + '_COLLISION')
        scene.collection.children.link(model)
        scene.collection.children.link(collision)
        copies = {}
        for source in sources:
            obj = source.copy()
            name = source.name.split('.')[0]
            obj.name = 'LASER_L%02d_%s' % (level, name)
            obj['canonical_name'] = name
            if source.type == 'MESH':
                obj.data = source.data.copy()
            (collision if name.startswith('COL_') else model).objects.link(obj)
            copies[source] = obj
        for source, obj in copies.items():
            obj.parent = copies.get(source.parent)
            obj.hide_set(False)
        objects = {o['canonical_name']: o for o in copies.values()}
        objects['asset_root']['upgrade_level'] = level
        objects['asset_root']['upgrade_mode'] = 'complete replacement, not an overlay'
        bpy.context.view_layer.update()
        upgrade(level, objects, model)
        bpy.ops.object.select_all(action='DESELECT')
        for obj in copies.values():
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects['base_mesh']
        path = OUT / ('laser_lvl%02d.glb' % level)
        bpy.ops.export_scene.gltf(filepath=str(path), use_selection=True, use_active_scene=True,
            use_visible=False, use_renderable=False, export_yup=True, export_normals=True,
            export_texcoords=True, export_materials='EXPORT', export_animations=False, export_extras=True)
        common.clean_export_names(path)
        objects['COL_BOX_BODY'].hide_set(True)
        common.studio(scene)
        scenes.append(scene)
    assert hashlib.sha256(BASE.read_bytes()).hexdigest() == original_hash
    bpy.context.window.scene = scenes[-1]
    bpy.data.libraries.write(str(ART / 'laser_levels.blend'), set(scenes), fake_user=True)
    print('LASER_LEVELS_BUILT', original_hash, flush=True)


def render(level, view='beauty'):
    scene = bpy.data.scenes['LASER_L%02d' % level]
    bpy.context.window.scene = scene
    pos, target, scale = {
        'beauty': ((2.6, 4.5, 2.6), (0, 0, .72), 2.2),
        'front': ((0, 5, .74), (0, 0, .74), 1.95),
        'side': ((5, 0, .74), (0, 0, .74), 1.95),
        'back': ((0, -5, .74), (0, 0, .74), 1.95),
        'top': ((0, .001, 5), (0, 0, 0), 1.75),
    }[view]
    scene.camera.location = pos
    common.aim(scene.camera, target)
    scene.camera.data.ortho_scale = scale
    scene.render.filepath = str(ART / ('level_%d_%s.png' % (level, view)))
    bpy.ops.render.render(write_still=True)


def overview():
    scene = bpy.data.scenes.new('LASER_LEVEL_OVERVIEW')
    bpy.context.window.scene = scene
    common.studio(scene)
    for level in range(1, 9):
        source = bpy.data.scenes['LASER_L%02d' % level]
        offset = Vector(((1.5 - (level - 1) % 4) * 1.75, 0, (1 - (level - 1) // 4) * 1.9))
        for obj in source.objects:
            if obj.type == 'MESH' and obj.get('canonical_name') != 'COL_BOX_BODY':
                copy = bpy.data.objects.new('preview_' + obj.name, obj.data)
                scene.collection.objects.link(copy)
                copy.matrix_world = Matrix.Translation(offset) @ obj.matrix_world
        label = bpy.data.curves.new('LASER_LEVEL_%d' % level, 'FONT')
        label.body, label.align_x, label.size = 'LEVEL %d' % level, 'CENTER', .13
        obj = bpy.data.objects.new(label.name, label)
        scene.collection.objects.link(obj)
        obj.location = (offset.x, .68, offset.z - .12)
        obj.rotation_euler = (math.pi / 2, 0, math.pi)
    scene.camera.location = (1.4, 13, 5.3)
    common.aim(scene.camera, (0, 0, 1.55))
    scene.camera.data.ortho_scale = 7.5
    scene.render.resolution_x, scene.render.resolution_y = 1800, 1100
    scene.render.filepath = str(ART / 'levels_1-8.png')
    bpy.ops.render.render(write_still=True)
    scene = bpy.data.scenes.new('LASER_FAMILY_COMPARISON')
    bpy.context.window.scene = scene
    common.studio(scene)
    common.geometry.import_visible(ROOT / 'assets/models/buildings/cannon.glb',
        lambda o: 'upgrade_' not in o.name or '_l8_' in o.name, -2.4)
    common.geometry.import_visible(OUT / 'laser_lvl01.glb', offset=-.8)
    common.geometry.import_visible(OUT / 'laser_lvl08.glb', offset=.8)
    common.geometry.import_visible(ROOT / 'assets/models/buildings/repair_station_levels/repair_station_lvl08.glb', offset=2.4)
    scene.camera.location = (1, 11, 4.6)
    common.aim(scene.camera, (0, 0, .72))
    scene.camera.data.ortho_scale = 7
    scene.render.resolution_x, scene.render.resolution_y = 1700, 650
    scene.render.filepath = str(ART / 'family_comparison.png')
    bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    if '--render' in sys.argv:
        for level in range(1, 9):
            render(level)
        for level in (1, 8):
            for view in ('front', 'side', 'back', 'top'):
                render(level, view)
        overview()
    else:
        build()

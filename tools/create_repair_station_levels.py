"""Author eight complete repair variants through Blender MCP or background bpy.

Only the new levels directory is written. Original repair station and gameplay stay intact.
"""
from pathlib import Path
import hashlib
import importlib.util
import math
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/repair_station_levels'
OUT = ROOT / 'assets/models/buildings/repair_station_levels'
BASE = ROOT / 'assets/models/buildings/repair_station.glb'
SOURCE = ROOT / 'art/models/repair_station.blend'
ART.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('repair_level_helpers', ROOT / 'tools/create_wind_blower_levels.py')
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
GOLD, RED, ORANGE, DARK = common.GOLD, common.RED, common.ORANGE, common.DARK
WRENCH_WIDTHS = (1, 1, 1, 1.12, 1.12, 1.12, 1.25, 1.35)
DESCRIPTIONS = (
    'Original repair station', 'Gold base belt, upper collar and console',
    'Gold braces and display frame', 'Larger cyan wrench and stem reinforcement rings',
    'Twin service reservoirs with cyan indicators', 'Rear service pipe and larger console deck',
    'Double base belt and reinforced reservoirs', 'Master wrench holder and heavy service frame',
)
scenes = {}


def upgrade(level, objects, collection):
    base, service = objects['base_mesh'], objects['service_mesh']
    g = common.geometry.Geometry()
    energy = common.geometry.Geometry()
    for index in common.group_vertices(service, 'repair_wrench'):
        service.data.vertices[index].co.x *= WRENCH_WIDTHS[level - 1]
    if level >= 2:
        for prefix in ('repair_upper_collar', 'repair_console_deck'):
            common.recolor(base, prefix, GOLD)
        g.ring('gold_foot_belt', .178, .225, .588, .625, GOLD)
    if level >= 3:
        for prefix in ('repair_brace_', 'repair_display_frame', 'repair_stem_seat'):
            # Leave the dark exposed bolt heads untouched.
            if prefix == 'repair_brace_':
                for i in range(4): common.recolor(base, 'repair_brace_%d' % i, GOLD)
            else: common.recolor(base, prefix, GOLD)
    if level >= 4:
        for i, z in enumerate((.678, .758, .824)):
            g.ring('gold_stem_reinforcement_%d' % i, z, z + .024, .100, .133, GOLD)
    if level >= 5:
        for side in (-1, 1):
            c = (side * .490, -.010)
            g.lathe('service_lower_cap_%d' % side, [(.091, .224), (.109, .249), (.109, .292), (.094, .309)], GOLD, c)
            g.lathe('service_reservoir_%d' % side, [(.089, .298), (.096, .320), (.096, .866), (.084, .887)], RED, c)
            g.lathe('service_upper_cap_%d' % side, [(.099, .865), (.111, .887), (.111, .931), (.090, .951)], GOLD, c)
            g.ring('service_band_%d' % side, .482, .512, .093, .108, GOLD, c)
            energy.lathe('service_indicator_%d' % side, [(.053, .946), (.053, .967), (.040, .981)], (.03125, .105, .205), c, 8)
    if level >= 6:
        for side in (-1, 1):
            common.axis_lathe(g, 'service_return_pipe_%d' % side, [(.028, -.230), (.028, -.010)], DARK, (side * .490, 0, .920), 8)
        start = len(g.verts)
        common.axis_lathe(g, 'service_cross_pipe', [(.028, -.490), (.028, .490)], DARK, (0, 0, .920), 8)
        for i in range(start, len(g.verts)):
            x, y, z = g.verts[i]
            g.verts[i] = (y, -.230 + x, z)
        for index in common.group_vertices(base, 'repair_console_deck'):
            base.data.vertices[index].co.x *= 1.15
    if level >= 7:
        g.ring('lower_master_belt', .097, .132, .588, .638, GOLD)
        g.ring('red_master_belt', .228, .265, .386, .421, RED)
        for side in (-1, 1):
            g.ring('service_upper_band_%d' % side, .718, .748, .093, .108, GOLD, (side * .490, -.010))
    if level >= 8:
        common.recolor(base, 'repair_symbol_socket', GOLD)
        g.lathe('master_wrench_holder', [(.105, .988), (.137, 1.007), (.137, 1.043), (.111, 1.064)], GOLD)
        for side in (-1, 1):
            common.box(g, 'orange_service_pipe_shell_%d' % side, (side * .405, -.230, .920), (.180, .074, .074), ORANGE)
            common.box(g, 'gold_service_pipe_joint_%d' % side, (side * .300, -.230, .920), (.042, .087, .087), GOLD)
        g.ring('master_upper_collar', .878, .909, .174, .204, GOLD)
    common.append_geometry(base, g, collection, base.data.materials[0])
    common.append_geometry(service, energy, collection, service.data.materials[0])
    objects['socket_ui_top'].location.z = 1.600
    if level > 1:
        for v in objects['COL_BOX_BODY'].data.vertices:
            v.co.x = math.copysign(.660, v.co.x)
            v.co.y = .730 if v.co.y > 0 else -.660
            v.co.z = 0 if v.co.z < .01 else 1.520
    base.data.update()
    service.data.update()


def build():
    assert not bpy.data.scenes.get('REPAIR_STATION_L01'), 'Preserve manual edits before rebuilding.'
    original_hash = hashlib.sha256(BASE.read_bytes()).hexdigest()
    if bpy.data.filepath and Path(bpy.data.filepath).resolve() == SOURCE.resolve():
        original = bpy.data.scenes['REPAIR_STATION']
    else:
        with bpy.data.libraries.load(str(SOURCE), link=False) as (available, loaded):
            loaded.scenes = ['REPAIR_STATION']
        original = loaded.scenes[0]
    sources = [o for o in original.objects if o.type not in ('LIGHT', 'CAMERA')]
    for level in range(1, 9):
        scene = bpy.data.scenes.new('REPAIR_STATION_L%02d' % level)
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
            obj.name = 'REPAIR_L%02d_%s' % (level, name)
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
        path = OUT / ('repair_station_lvl%02d.glb' % level)
        bpy.ops.export_scene.gltf(filepath=str(path), use_selection=True, use_active_scene=True,
                                 use_visible=False, use_renderable=False, export_yup=True, export_normals=True,
                                 export_texcoords=True, export_materials='EXPORT', export_animations=False, export_extras=True)
        common.clean_export_names(path)
        common.studio(scene)
        scene.camera.data.ortho_scale = 2.25
        common.aim(scene.camera, (0, 0, .78))
        scenes[level] = scene
    assert hashlib.sha256(BASE.read_bytes()).hexdigest() == original_hash
    bpy.context.window.scene = scenes[8]
    bpy.data.libraries.write(str(ART / 'repair_station_levels.blend'), set(scenes.values()), fake_user=True)
    print('REPAIR_LEVELS_BUILT', original_hash, flush=True)


def render(level, view='beauty'):
    scene = bpy.data.scenes['REPAIR_STATION_L%02d' % level]
    bpy.context.window.scene = scene
    pos, target, scale = {
        'beauty': ((2.6, 4.5, 2.6), (0, 0, .78), 2.30),
        'front': ((0, 5, .78), (0, 0, .78), 2.05),
        'side': ((5, 0, .78), (0, 0, .78), 2.05),
        'back': ((0, -5, .78), (0, 0, .78), 2.05),
        'top': ((0, .001, 5), (0, 0, 0), 1.80),
    }[view]
    scene.camera.location = pos
    common.aim(scene.camera, target)
    scene.camera.data.ortho_scale = scale
    scene.render.filepath = str(ART / ('level_%d_%s.png' % (level, view)))
    bpy.ops.render.render(write_still=True)


def overview():
    scene = bpy.data.scenes.new('REPAIR_LEVEL_OVERVIEW')
    bpy.context.window.scene = scene
    common.studio(scene)
    for level in range(1, 9):
        source = bpy.data.scenes['REPAIR_STATION_L%02d' % level]
        offset = Vector(((1.5 - (level - 1) % 4) * 1.75, 0, (1 - (level - 1) // 4) * 2.0))
        for obj in source.objects:
            if obj.type == 'MESH' and obj.get('canonical_name') != 'COL_BOX_BODY':
                copy = bpy.data.objects.new('preview_' + obj.name, obj.data)
                scene.collection.objects.link(copy)
                copy.matrix_world = Matrix.Translation(offset) @ obj.matrix_world
        label = bpy.data.curves.new('REPAIR_LEVEL_%d' % level, 'FONT')
        label.body, label.align_x, label.size = 'LEVEL %d' % level, 'CENTER', .13
        obj = bpy.data.objects.new(label.name, label)
        scene.collection.objects.link(obj)
        obj.location = (offset.x, .7, offset.z - .12)
        obj.rotation_euler = (math.pi / 2, 0, math.pi)
    scene.camera.location = (1.0, 13, 5.0)
    common.aim(scene.camera, (0, 0, 1.65))
    scene.camera.data.ortho_scale = 7.6
    scene.render.resolution_x, scene.render.resolution_y = 1800, 1200
    scene.render.filepath = str(ART / 'levels_1-8.png')
    bpy.ops.render.render(write_still=True)
    scene = bpy.data.scenes.new('REPAIR_FAMILY_COMPARISON')
    bpy.context.window.scene = scene
    common.studio(scene)
    common.geometry.import_visible(ROOT / 'assets/models/buildings/tesla_tower.glb', offset=-2.4)
    common.geometry.import_visible(ROOT / 'assets/models/buildings/tesla_upgrades.glb', lambda o: o.name.startswith('tesla_upgrade_l8'), -2.4)
    common.geometry.import_visible(OUT / 'repair_station_lvl01.glb', offset=-.8)
    common.geometry.import_visible(OUT / 'repair_station_lvl08.glb', offset=.8)
    common.geometry.import_visible(ROOT / 'assets/models/buildings/wind_blower_levels/wind_blower_lvl08.glb', offset=2.4)
    scene.camera.location = (1, 11, 4.7)
    common.aim(scene.camera, (0, 0, .76))
    scene.camera.data.ortho_scale = 7.1
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

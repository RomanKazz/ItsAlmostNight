"""Author eight complete decoy variants through Blender MCP or background bpy.

Only the new levels directory is written. Original beacon and gameplay stay intact.
"""
from pathlib import Path
import hashlib
import importlib.util
import math
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/decoy_beacon_levels'
OUT = ROOT / 'assets/models/buildings/decoy_beacon_levels'
BASE = ROOT / 'assets/models/buildings/decoy_beacon.glb'
SOURCE = ROOT / 'art/models/decoy_beacon.blend'
ART.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('decoy_level_helpers', ROOT / 'tools/create_wind_blower_levels.py')
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
GOLD, RED, ORANGE, DARK = common.GOLD, common.RED, common.ORANGE, common.DARK
CENTRAL_SCALES = (1, 1, 1, 1.18, 1.18, 1.18, 1.35, 1.35)
LAMP_SCALES = (1, 1, 1, 1, 1, 1.14, 1.14, 1.27)
DESCRIPTIONS = (
    'Original five-lantern beacon', 'Gold lantern caps, lifting eye and base belt',
    'Gold structural feet with exposed orange inlays', 'Wider central lamp and lower protective collar',
    'Broad crown housing and reinforced cross arms', 'Larger hanging lamps with protective corner bars',
    'Four red pulse capacitors and a larger central lantern', 'Oversized lanterns, master crown and double base band',
)
scenes = {}


def radial_scale(obj, prefixes, scale, center=(0, 0)):
    chosen = set().union(*(common.group_vertices(obj, p) for p in prefixes))
    for v in obj.data.vertices:
        if v.index in chosen:
            v.co.x = center[0] + (v.co.x - center[0]) * scale
            v.co.y = center[1] + (v.co.y - center[1]) * scale


def upgrade(level, objects, collection):
    base, light = objects['base_mesh'], objects['light_mesh']
    g = common.geometry.Geometry()
    scale = CENTRAL_SCALES[level - 1]
    radial_scale(base, ['decoy_center_'], scale)
    radial_scale(light, ['decoy_center_glass'], scale)
    lamp_scale = LAMP_SCALES[level - 1]
    for i in range(4):
        angle = i * math.pi / 2
        center = Vector((-math.sin(angle) * .505, math.cos(angle) * .505, .810))
        for obj in (base, light):
            prefixes = ['decoy_lamp_%s_%02d' % (part, i) for part in ('top', 'bottom', 'glass')]
            chosen = set().union(*(common.group_vertices(obj, p) for p in prefixes))
            for v in obj.data.vertices:
                if v.index in chosen:
                    v.co = center + (v.co - center) * lamp_scale
        objects['socket_light_%02d' % (i + 1)].location.z = .810 + (.6705 - .810) * lamp_scale
    if level >= 2:
        for prefix in ('decoy_center_lower_cap', 'decoy_center_upper_cap', 'decoy_lamp_top_',
                       'decoy_lamp_bottom_', 'decoy_lifting_eye', 'decoy_mast_collar'):
            common.recolor(base, prefix, GOLD)
        g.ring('gold_base_belt', .120, .168, .388, .421, GOLD)
    if level >= 3:
        common.recolor(base, 'decoy_foot_', GOLD)
        common.recolor(base, 'decoy_arm_joint_', GOLD)
        common.recolor(base, 'decoy_lower_socket', GOLD)
    if level >= 4:
        common.recolor(base, 'decoy_center_guard_', GOLD)
        g.ring('center_guard_foot', .393, .432, .125 * scale, .180 * scale, GOLD)
    if level >= 5:
        radial_scale(base, ['decoy_head'], 1.18)
        g.ring('crown_gold_bezel', 1.167, 1.202, .216, .280, GOLD)
        for i in range(4):
            angle = i * math.pi / 2
            start = len(g.verts)
            common.box(g, 'orange_crossarm_shell_%02d' % i, (0, .325, 1.062), (.080, .300, .084), ORANGE)
            for j in range(start, len(g.verts)):
                x, y, z = g.verts[j]
                g.verts[j] = (x * math.cos(angle) - y * math.sin(angle),
                              x * math.sin(angle) + y * math.cos(angle), z)
    if level >= 6:
        for i in range(4):
            angle = i * math.pi / 2
            x, y = -math.sin(angle) * .505, math.cos(angle) * .505
            zlo = .810 + (.585 - .810) * lamp_scale
            zhi = .810 + (.748 - .810) * lamp_scale
            radius = .098 * .90 * lamp_scale + .006
            for corner in range(4):
                a = math.pi / 4 + corner * math.pi / 2
                common.box(g, 'lamp_guard_%02d_%02d' % (i, corner),
                           (x + radius * math.cos(a), y + radius * math.sin(a), (zlo + zhi) / 2),
                           (.014, .014, zhi - zlo), GOLD)
    if level >= 7:
        for i in range(4):
            angle = math.pi / 4 + i * math.pi / 2
            center = (.320 * math.cos(angle), .320 * math.sin(angle))
            g.lathe('pulse_capacitor_seat_%02d' % i, [(.060, .304), (.078, .329), (.078, .358)], GOLD, center, 8)
            g.lathe('pulse_capacitor_body_%02d' % i, [(.060, .349), (.065, .368), (.065, .585), (.058, .605)], RED, center, 8)
            g.lathe('pulse_capacitor_cap_%02d' % i, [(.068, .581), (.077, .601), (.077, .631), (.057, .649)], GOLD, center, 8)
    if level >= 8:
        g.ring('lower_master_belt', .045, .080, .388, .421, GOLD)
        g.ring('red_master_belt', .173, .211, .382, .412, RED)
        g.ring('crown_lower_belt', 1.078, 1.115, .216, .280, GOLD)
        for i in range(4):
            angle = i * math.pi / 2
            start = len(g.verts)
            common.box(g, 'crown_radial_rank_%02d' % i, (0, .263, 1.140), (.057, .036, .045), RED)
            for j in range(start, len(g.verts)):
                x, y, z = g.verts[j]
                g.verts[j] = (x * math.cos(angle) - y * math.sin(angle),
                              x * math.sin(angle) + y * math.cos(angle), z)
        eye_vertices = common.group_vertices(base, 'decoy_lifting_eye')
        for v in base.data.vertices:
            if v.index in eye_vertices:
                v.co.x *= 1.20
                v.co.y *= 1.20
                v.co.z = 1.342 + (v.co.z - 1.342) * 1.20
    common.append_geometry(base, g, collection, base.data.materials[0])
    objects['socket_ui_top'].location.z = 1.620
    if level > 1:
        for v in objects['COL_BOX_BODY'].data.vertices:
            v.co.x = math.copysign(.670, v.co.x)
            v.co.y = math.copysign(.670, v.co.y)
            v.co.z = 0 if v.co.z < .01 else 1.530
    base.data.update()
    light.data.update()


def build():
    assert not bpy.data.scenes.get('DECOY_BEACON_L01'), 'Preserve manual edits before rebuilding.'
    original_hash = hashlib.sha256(BASE.read_bytes()).hexdigest()
    if bpy.data.filepath and Path(bpy.data.filepath).resolve() == SOURCE.resolve():
        original = bpy.data.scenes['DECOY_BEACON']
    else:
        with bpy.data.libraries.load(str(SOURCE), link=False) as (available, loaded):
            loaded.scenes = ['DECOY_BEACON']
        original = loaded.scenes[0]
    sources = [o for o in original.objects if o.type not in ('LIGHT', 'CAMERA')]
    for level in range(1, 9):
        scene = bpy.data.scenes.new('DECOY_BEACON_L%02d' % level)
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
            obj.name = 'DECOY_L%02d_%s' % (level, name)
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
        path = OUT / ('decoy_beacon_lvl%02d.glb' % level)
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
    bpy.data.libraries.write(str(ART / 'decoy_beacon_levels.blend'), set(scenes.values()), fake_user=True)
    print('DECOY_LEVELS_BUILT', original_hash, flush=True)


def render(level, view='beauty'):
    scene = bpy.data.scenes['DECOY_BEACON_L%02d' % level]
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
    scene = bpy.data.scenes.new('DECOY_LEVEL_OVERVIEW')
    bpy.context.window.scene = scene
    common.studio(scene)
    for level in range(1, 9):
        source = bpy.data.scenes['DECOY_BEACON_L%02d' % level]
        offset = Vector(((1.5 - (level - 1) % 4) * 1.75, 0, (1 - (level - 1) // 4) * 2.0))
        for obj in source.objects:
            if obj.type == 'MESH' and obj.get('canonical_name') != 'COL_BOX_BODY':
                copy = bpy.data.objects.new('preview_' + obj.name, obj.data)
                scene.collection.objects.link(copy)
                copy.matrix_world = Matrix.Translation(offset) @ obj.matrix_world
        label = bpy.data.curves.new('DECOY_LEVEL_%d' % level, 'FONT')
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
    scene = bpy.data.scenes.new('DECOY_FAMILY_COMPARISON')
    bpy.context.window.scene = scene
    common.studio(scene)
    common.geometry.import_visible(ROOT / 'assets/models/buildings/tesla_tower.glb', offset=-2.4)
    common.geometry.import_visible(ROOT / 'assets/models/buildings/tesla_upgrades.glb', lambda o: o.name.startswith('tesla_upgrade_l8'), -2.4)
    common.geometry.import_visible(OUT / 'decoy_beacon_lvl01.glb', offset=-.8)
    common.geometry.import_visible(OUT / 'decoy_beacon_lvl08.glb', offset=.8)
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

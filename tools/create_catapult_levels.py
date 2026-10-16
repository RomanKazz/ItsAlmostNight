"""Blender-authored catapult overlays, retaining the exact original GLB rig.

Build through Blender MCP, or a fresh background bpy process. --render renders
the saved editable eight-scene source. The original base is an immutable input.
"""
from pathlib import Path
import copy
from datetime import datetime
import importlib.util
import json
import math
import shutil
import struct
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/catapult_levels'
BASE = ART / 'base_catapult.glb'
OUT = ROOT / 'assets/models/buildings/catapult.glb'
TEMP = ROOT / 'output/catapult_levels/overlays.glb'
ART.mkdir(parents=True, exist_ok=True)
TEMP.parent.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('catapult_level_helpers', ROOT / 'tools/create_wind_blower_levels.py')
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
GOLD, RED, ORANGE, DARK = common.GOLD, common.RED, common.ORANGE, common.DARK
PIVOT = .14931100606918335
BUCKET = .6366119384765625
DESCRIPTIONS = (
    'Original catapult', 'Gold foot belt and bucket binding',
    'Reinforced gold carriage cheeks', 'Gold bucket rim',
    'Red bucket binding and gold axle collars', 'Twin torsion drums with gold caps',
    'Raised heavy bucket rim and double base belt', 'Gold bucket clamps and raised orange cheek inlays',
)


def read_glb(path):
    raw = path.read_bytes()
    length = struct.unpack_from('<I', raw, 12)[0]
    size = struct.unpack_from('<I', raw, 20 + length)[0]
    return json.loads(raw[20:20 + length]), raw[28 + length:28 + length + size]


def write_glb(path, doc, binary):
    doc['buffers'][0]['byteLength'] = len(binary)
    data = json.dumps(doc, separators=(',', ':')).encode()
    data += b' ' * (-len(data) % 4)
    binary = bytes(binary) + b'\0' * (-len(binary) % 4)
    path.write_bytes(struct.pack('<III', 0x46546c67, 2, 28 + len(data) + len(binary)) +
                     struct.pack('<II', len(data), 0x4e4f534a) + data +
                     struct.pack('<II', len(binary), 0x004e4942) + binary)


def foot_band(g, name, lo, hi, inner, outer, color):
    # Leave a rear clearance slot for the returning lever.
    profile = ((inner, lo), (outer, lo), (outer, hi), (inner, hi))
    vertices = [(r * math.cos(a), r * math.sin(a), z)
                for i in range(17) for a in [math.radians(110 + 20 * i)] for r, z in profile]
    faces = [(4 * i + j, 4 * i + (j + 1) % 4,
              4 * (i + 1) + (j + 1) % 4, 4 * (i + 1) + j)
             for i in range(16) for j in range(4)]
    faces += [(3, 2, 1, 0), (64, 65, 66, 67)]
    g.add(name, vertices, faces, color, [i for i in range(64) if i % 4 in (1, 3)])


def details(level):
    body, arm = common.geometry.Geometry(), common.geometry.Geometry()
    foot_band(body, 'gold_foot_belt', .100, .146, .588, .625, GOLD)
    arm.ring('bucket_lower_binding', .690, .722, .350, .412, RED if level >= 5 else GOLD, (0, -BUCKET), 8)
    if level >= 3:
        outline = [(-.220, .225), (.220, .225), (.190, .660),
                   (.090, .820), (-.090, .820), (-.190, .660)]
        for side in (-1, 1):
            lo, hi = sorted((side * .450, side * .540))
            common.side_panel(body, 'gold_cheek_%d' % side, lo, hi, outline, GOLD)
    if level >= 4:
        top, radius = (.955, .435) if level >= 7 else (.910, .415)
        arm.ring('bucket_gold_rim', .855, top, .280, radius, GOLD, (0, -BUCKET), 8)
    if level >= 5:
        for side in (-1, 1):
            # Rings around X, outside the existing dark axle cap.
            start = len(body.verts)
            common.axis_lathe(body, 'gold_axle_collar_%d' % side,
                [(.122, -.015), (.165, -.015), (.165, .015), (.122, .015)], GOLD,
                (0, 0, 0), 8, hollow=True)
            for i in range(start, len(body.verts)):
                x, y, z = body.verts[i]
                body.verts[i] = (side * .558 + y, -z, .4762 + x)
    if level >= 6:
        for side in (-1, 1):
            for name, profile, color in (
                ('torsion_drum', [(.106, .552), (.138, .580), (.138, .702), (.116, .724)], RED),
                ('torsion_end_cap', [(.132, .702), (.158, .716), (.158, .742), (.120, .755)], GOLD),
                ('torsion_end_bolt', [(.053, .754), (.053, .775)], DARK),
            ):
                start = len(body.verts)
                common.axis_lathe(body, name + '_%d' % side, profile, color, (0, 0, 0), 8)
                for i in range(start, len(body.verts)):
                    x, y, z = body.verts[i]
                    body.verts[i] = (side * y, -z, .4762 + x)
    if level >= 7:
        foot_band(body, 'lower_master_belt', .068, .098, .588, .638, GOLD)
    if level >= 8:
        foot_band(body, 'red_master_belt', .151, .187, .588, .626, RED)
        for side in (-1, 1):
            lo, hi = sorted((side * .539, side * .550))
            common.side_panel(body, 'raised_orange_cheek_%d' % side, lo, hi,
                [(-.165, .293), (.165, .293), (.138, .604),
                 (.063, .739), (-.063, .739), (-.138, .604)], ORANGE)
        for i in range(8):
            angle = math.pi / 8 + i * math.pi / 4
            start = len(arm.verts)
            common.box(arm, 'bucket_clamp_%d' % i, (0, .373, .8075), (.055, .025, .095), GOLD)
            for j in range(start, len(arm.verts)):
                x, y, z = arm.verts[j]
                arm.verts[j] = (x * math.cos(angle) - y * math.sin(angle),
                                -BUCKET + x * math.sin(angle) + y * math.cos(angle), z)
    return body, arm


def assemble_runtime():
    doc, binary = read_glb(BASE)
    binary = bytearray(binary[:doc['buffers'][0]['byteLength']])
    extra, payload = read_glb(TEMP)
    nodes = {n['name']: n for n in extra['nodes']}
    views, accessors, bounds = {}, {}, []

    def accessor(index):
        if index in accessors:
            return accessors[index]
        row = copy.deepcopy(extra['accessors'][index])
        assert 'sparse' not in row
        source = row['bufferView']
        if source not in views:
            view = copy.deepcopy(extra['bufferViews'][source])
            start, size = view.get('byteOffset', 0), view['byteLength']
            binary.extend(b'\0' * (-len(binary) % 4))
            view['buffer'], view['byteOffset'] = 0, len(binary)
            binary.extend(payload[start:start + size])
            views[source] = len(doc['bufferViews'])
            doc['bufferViews'].append(view)
        row['bufferView'] = views[source]
        accessors[index] = len(doc['accessors'])
        doc['accessors'].append(row)
        return accessors[index]

    for level in range(2, 9):
        for role in ('base', 'arm'):
            name = 'upgrade_catapult_l%d_%s' % (level, role)
            node = nodes[name]
            assert all(key not in node for key in ('matrix', 'translation', 'rotation', 'scale'))
            parts = extra['meshes'][node['mesh']]['primitives']
            assert len(parts) == 1
            primitive = copy.deepcopy(parts[0])
            primitive['attributes'] = {k: accessor(v) for k, v in primitive['attributes'].items()}
            primitive['indices'] = accessor(primitive['indices'])
            # The original material already references the same Kenney PNG.
            primitive['material'] = 0
            position = doc['accessors'][primitive['attributes']['POSITION']]
            bounds.append((position['min'], position['max']))
            doc['meshes'].append({'name': name, 'primitives': [primitive]})
            doc['scenes'][doc.get('scene', 0)]['nodes'].append(len(doc['nodes']))
            doc['nodes'].append({'name': name, 'mesh': len(doc['meshes']) - 1})
    write_glb(OUT, doc, binary)
    # Only the generated bounds/comment change; preserve the runtime pose API.
    header_path = ROOT / 'src/graphics/CatapultModel.hpp'
    header = header_path.read_text()
    start = header.index('inline constexpr std::array<BoundingBox, 14> UpgradeBounds{{')
    end = header.index('}};', start) + 3
    def vec(v):
        return '{' + ', '.join('%.6fF' % c for c in v) + '}'
    table = 'inline constexpr std::array<BoundingBox, 14> UpgradeBounds{{\n'
    table += ''.join('    {' + vec(lo) + ', ' + vec(hi) + '},\n' for lo, hi in bounds) + '}};'
    header = header[:start] + table + header[end:]
    header = header.replace('tools/generate_catapult_upgrades.py', 'tools/create_catapult_levels.py')
    header_path.write_text(header)


def build():
    assert not bpy.data.scenes.get('CATAPULT_L01'), 'Preserve manual level edits before rebuilding.'
    original = bpy.data.scenes.get('CATAPULT_BASE_REFERENCE')
    if original is None:
        original = bpy.data.scenes.new('CATAPULT_BASE_REFERENCE')
        bpy.context.window.scene = original
        bpy.ops.import_scene.gltf(filepath=str(BASE))
    sources = [o for o in original.objects if o.type not in ('LIGHT', 'CAMERA')]
    original_material = next(o for o in sources if o.type == 'MESH' and not o.name.startswith('COL_')).data.materials[0]
    scenes, overlays = [], []
    for level in range(1, 9):
        scene = bpy.data.scenes.new('CATAPULT_L%02d' % level)
        bpy.context.window.scene = scene
        scene.unit_settings.system, scene.unit_settings.scale_length = 'METRIC', 1
        model = bpy.data.collections.new(scene.name + '_MODEL')
        collision = bpy.data.collections.new(scene.name + '_COLLISION')
        scene.collection.children.link(model)
        scene.collection.children.link(collision)
        copies = {}
        for source in sources:
            obj = source.copy()
            canonical = source.name
            obj.name = 'CATAPULT_L%02d_%s' % (level, canonical)
            obj['canonical_name'] = canonical
            if source.type == 'MESH':
                obj.data = source.data.copy()
            (collision if canonical.startswith('COL_') else model).objects.link(obj)
            copies[source] = obj
        for source, obj in copies.items():
            obj.parent = copies.get(source.parent)
        bpy.context.view_layer.update()
        for obj in copies.values():
            if obj.type == 'MESH':
                if obj.name.split('_', 2)[-1].startswith('COL_'):
                    obj.hide_render = True
                    obj.hide_set(True)
                elif obj['canonical_name'].startswith('catapult_'):
                    obj['role'] = 'original_arm'
                else:
                    obj['role'] = 'original_carriage'
        if level > 1:
            body, arm = details(level)
            pitch = next(o for o in copies.values() if o['canonical_name'].startswith('pitch_pivot'))
            yaw = next(o for o in copies.values() if o['canonical_name'].startswith('yaw_pivot'))
            for role, geometry in (('base', body), ('arm', arm)):
                obj = geometry.object('upgrade_catapult_l%d_%s' % (level, role), model, original_material)
                obj['canonical_name'], obj['role'] = obj.name, role
                parent = pitch if role == 'arm' else yaw
                obj.parent = parent
                obj.matrix_parent_inverse = parent.matrix_world.inverted()
                overlays.append(obj)
        common.studio(scene)
        scene.camera.location = (2.6, -4.5, 2.6)
        common.aim(scene.camera, (0, -.15, .55))
        scene.camera.data.ortho_scale = 2.3
        scenes.append(scene)
    export_scene = bpy.data.scenes.new('CATAPULT_OVERLAY_EXPORT')
    bpy.context.window.scene = export_scene
    for original in overlays:
        obj = original.copy()
        obj.parent = None
        obj.matrix_world = Matrix.Identity(4)
        export_scene.collection.objects.link(obj)
        obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(TEMP), use_selection=True, use_active_scene=True,
        export_yup=True, export_normals=True, export_texcoords=True, export_materials='EXPORT',
        export_animations=False, export_extras=True)
    common.clean_export_names(TEMP)
    assemble_runtime()
    bpy.context.window.scene = scenes[-1]
    source = ART / 'catapult_levels.blend'
    if source.exists():
        shutil.copy2(source, TEMP.parent / ('source_before_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f') + '.blend'))
    bpy.data.libraries.write(str(source), set(scenes), fake_user=True)
    print('CATAPULT_LEVELS_BUILT', flush=True)


def render(level, view='beauty'):
    scene = bpy.data.scenes['CATAPULT_L%02d' % level]
    bpy.context.window.scene = scene
    pos, target, scale = {
        'beauty': ((2.6, -4.5, 2.6), (0, -.15, .55), 2.35),
        'front': ((0, -5, .60), (0, -.15, .60), 2.1),
        'side': ((5, 0, .60), (0, -.15, .60), 2.1),
        'back': ((0, 5, .60), (0, -.15, .60), 2.1),
        'top': ((0, -.001, 5), (0, -.15, 0), 2.1),
    }[view]
    scene.camera.location = pos
    common.aim(scene.camera, target)
    scene.camera.data.ortho_scale = scale
    scene.render.filepath = str(ART / ('level_%d_%s.png' % (level, view)))
    bpy.ops.render.render(write_still=True)


def overview():
    scene = bpy.data.scenes.new('CATAPULT_OVERVIEW')
    bpy.context.window.scene = scene
    common.studio(scene)
    for level in range(1, 9):
        source = bpy.data.scenes['CATAPULT_L%02d' % level]
        offset = Vector((((level - 1) % 4 - 1.5) * 1.8, 0, (1 - (level - 1) // 4) * 1.8))
        for obj in source.objects:
            if obj.type == 'MESH' and not obj.get('canonical_name', '').startswith('COL_'):
                clone = bpy.data.objects.new('preview_' + obj.name, obj.data)
                scene.collection.objects.link(clone)
                clone.matrix_world = Matrix.Translation(offset) @ obj.matrix_world
        label = bpy.data.curves.new('CATAPULT_LEVEL_%d' % level, 'FONT')
        label.body, label.align_x, label.size = 'LEVEL %d' % level, 'CENTER', .13
        obj = bpy.data.objects.new(label.name, label)
        scene.collection.objects.link(obj)
        obj.location = (offset.x, -.95, offset.z - .14)
        obj.rotation_euler = (math.pi / 2, 0, 0)
    scene.camera.location = (1.0, -14, 6.8)
    common.aim(scene.camera, (0, -.1, 1.3))
    scene.camera.data.ortho_scale = 7.7
    scene.render.resolution_x, scene.render.resolution_y = 1800, 1100
    scene.render.filepath = str(ART / 'levels_1-8.png')
    bpy.ops.render.render(write_still=True)
    scene = bpy.data.scenes.new('CATAPULT_FAMILY_COMPARISON')
    bpy.context.window.scene = scene
    common.studio(scene)
    # Use source geometry, so hidden levels are never drawn in the comparison.
    for level, offset in ((1, -.8), (8, .8)):
        for obj in bpy.data.scenes['CATAPULT_L%02d' % level].objects:
            if obj.type == 'MESH' and not obj.get('canonical_name', '').startswith('COL_'):
                clone = bpy.data.objects.new('family_' + obj.name, obj.data)
                scene.collection.objects.link(clone)
                clone.matrix_world = Matrix.Translation((offset, 0, 0)) @ obj.matrix_world
    for path, offset in (
        ('assets/models/buildings/laser_levels/laser_lvl08.glb', -2.4),
        ('assets/models/buildings/repair_station_levels/repair_station_lvl08.glb', 2.4),
    ):
        for obj in common.geometry.import_visible(ROOT / path, offset=offset):
            obj.matrix_world = Matrix.Translation((offset, 0, 0)) @ Matrix.Rotation(math.pi, 4, 'Z') @ \
                Matrix.Translation((-offset, 0, 0)) @ obj.matrix_world
    scene.camera.location = (1, -12, 5.2)
    common.aim(scene.camera, (0, -.1, .55))
    scene.camera.data.ortho_scale = 7.2
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

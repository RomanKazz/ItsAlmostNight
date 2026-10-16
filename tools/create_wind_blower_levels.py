"""Eight complete wind-blower variants; no replacement of the integrated base.

Run build() through Blender MCP, or this script in a fresh background Blender.
Render with --render from a separate process. Original palette and rig are retained.
"""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import struct
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/wind_blower_levels'
OUT = ROOT / 'assets/models/buildings/wind_blower_levels'
BASE = ROOT / 'assets/models/buildings/wind_blower.glb'
SOURCE = ROOT / 'art/models/wind_blower.blend'
PALETTE = ROOT / 'assets/models/buildings/Textures/colormap.png'
ART.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('wind_level_geometry', ROOT / 'tools/create_tesla_upgrades.py')
geometry = importlib.util.module_from_spec(spec)
spec.loader.exec_module(geometry)
GOLD, RED, ORANGE, DARK = geometry.GOLD, geometry.RED, geometry.ORANGE, geometry.DARK
STEEL = (.84375, .025, .225)
SCALES = (1, 1, 1, 1.08, 1.14, 1.14, 1.20, 1.20)
DESCRIPTIONS = (
    'Original four-blade fan', 'Gold intake rim and foot belt',
    'Gold yoke with raised orange side panels', 'Larger intake and rear reinforcing collar',
    'Twin red motor banks and widened head', 'Six blades and an extended rear motor',
    'Heavy duct rails and double motor straps', 'Eight blades, flared intake and master pedestal',
)
scenes = {}


def aim(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def studio(scene):
    col = bpy.data.collections.new(scene.name + '_STUDIO')
    scene.collection.children.link(col)
    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = scene.render.resolution_y = 700
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.exposure = -.35
    scene.world = bpy.data.worlds.new(scene.name + '_WORLD')
    scene.world.use_nodes = True
    background = next(n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND')
    background.inputs['Color'].default_value = (.12, .16, .20, 1)
    background.inputs['Strength'].default_value = .65
    for name, pos, energy, size in (
        ('KEY', (2, 4, 6), 440, 6), ('FILL', (-4, 2, 4), 300, 6), ('RIM', (0, -4, 5), 400, 4),
    ):
        data = bpy.data.lights.new(scene.name + '_' + name, 'AREA')
        data.energy, data.size = energy, size
        obj = bpy.data.objects.new(data.name, data)
        col.objects.link(obj)
        obj.location = pos
        aim(obj, (0, 0, .72))
    data = bpy.data.cameras.new(scene.name + '_CAMERA')
    camera = bpy.data.objects.new(data.name, data)
    col.objects.link(camera)
    scene.camera = camera
    data.type = 'ORTHO'
    data.ortho_scale = 2.1
    camera.location = (2.6, 4.5, 2.6)
    aim(camera, (0, 0, .73))


def group_vertices(obj, prefix):
    indices = {g.index for g in obj.vertex_groups if g.name.startswith(prefix)}
    return {v.index for v in obj.data.vertices if any(g.group in indices for g in v.groups)}


def recolor(obj, prefix, swatch):
    ids = group_vertices(obj, prefix)
    for polygon in obj.data.polygons:
        if not all(i in ids for i in polygon.vertices):
            continue
        heights = [obj.data.vertices[i].co.z for i in polygon.vertices]
        lo, hi = min(heights), max(heights)
        for li in polygon.loop_indices:
            z = obj.data.vertices[obj.data.loops[li].vertex_index].co.z
            fraction = (z - lo) / (hi - lo) if hi - lo > 1e-6 else .65
            obj.data.uv_layers.active.data[li].uv = (swatch[0], swatch[1] + fraction * (swatch[2] - swatch[1]))


def remove_groups(obj, prefixes):
    import bmesh
    ids = set().union(*(group_vertices(obj, p) for p in prefixes))
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.index in ids], context='VERTS')
    bm.to_mesh(obj.data)
    bm.free()
    for group in list(obj.vertex_groups):
        if group.name.startswith(tuple(prefixes)):
            obj.vertex_groups.remove(group)


def axis_lathe(g, name, profile, swatch, center=(0, 0, .960), sides=12, hollow=False):
    """Y-axis body in Blender world coordinates, +Y towards the player."""
    if hollow:
        start = len(g.verts)
        vertices = [(r * math.cos(math.tau * i / sides), r * math.sin(math.tau * i / sides), y)
                    for r, y in profile for i in range(sides)]
        faces = []
        smooth = []
        for j in range(len(profile)):
            for i in range(sides):
                nxt = (j + 1) % len(profile)
                faces.append((j * sides + i, j * sides + (i + 1) % sides,
                              nxt * sides + (i + 1) % sides, nxt * sides + i))
                if abs(profile[j][1] - profile[nxt][1]) > 1e-6:
                    smooth.append(len(faces) - 1)
        g.add(name, vertices, faces, swatch, smooth)
    else:
        start = len(g.verts)
        g.lathe(name, profile, swatch, sides=sides)
    for i in range(start, len(g.verts)):
        x, y, z = g.verts[i]
        g.verts[i] = (x + center[0], z + center[1], -y + center[2])


def box(g, name, center, size, swatch):
    x, y, z = center
    hx, hy, hz = [v / 2 for v in size]
    vertices = [(x + dx, y + dy, z + dz) for dz in (-hz, hz) for dy in (-hy, hy) for dx in (-hx, hx)]
    g.add(name, vertices, [(0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4),
                         (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)], swatch)


def side_panel(g, name, xlo, xhi, outline, swatch):
    count = len(outline)
    verts = [(x, y, z) for x in (xlo, xhi) for y, z in outline]
    faces = [tuple(reversed(range(count))), tuple(count + i for i in range(count))]
    faces += [(i, (i + 1) % count, (i + 1) % count + count, i + count) for i in range(count)]
    g.add(name, verts, faces, swatch)


def append_geometry(base, g, collection, material):
    if not g.verts:
        return
    extra = g.object('wind_level_detail', collection, material)
    bpy.context.view_layer.update()
    extra.data.transform(base.matrix_world.inverted())
    extra.parent = base.parent
    extra.matrix_basis = base.matrix_basis.copy()
    bpy.ops.object.select_all(action='DESELECT')
    base.select_set(True)
    extra.select_set(True)
    bpy.context.view_layer.objects.active = base
    bpy.ops.object.join()


def upgrade(level, objects, collection):
    base, mount, rotor = (objects[k] for k in ('base_mesh', 'yaw_mesh', 'rotor_mesh'))
    scale = SCALES[level - 1]
    material = base.data.materials[0]
    gs = {role: geometry.Geometry() for role in ('base', 'yaw', 'rotor')}
    if scale != 1:
        for v in mount.data.vertices:
            groups = {mount.vertex_groups[g.group].name for g in v.groups}
            world = mount.matrix_world @ v.co
            if any(name.startswith(('wind_duct', 'wind_front', 'wind_barrel', 'wind_rim', 'wind_motor', 'wind_rear')) for name in groups):
                world.x *= scale
                world.z = .960 + (world.z - .960) * scale
            elif any(name.startswith(('wind_u_yoke', 'wind_side_')) for name in groups):
                world.x *= scale
            elif 'wind_yoke_seat' in groups:
                world.x *= min(scale, 1.12)
                world.y *= min(scale, 1.12)
            v.co = mount.matrix_world.inverted() @ world
        for v in rotor.data.vertices:
            v.co.x *= scale
            v.co.z *= scale
    if level >= 2:
        recolor(mount, 'wind_front_rim', GOLD)
        recolor(rotor, 'wind_rotor_hub', GOLD)
        gs['base'].ring('gold_foot_belt', .136, .183, .588, .625, GOLD)
    if level >= 3:
        recolor(mount, 'wind_u_yoke', GOLD)
        recolor(base, 'wind_base_brace_', GOLD)
        for side in (-1, 1):
            lo, hi = sorted((side * .431 * scale, side * .452 * scale))
            side_panel(gs['yaw'], 'orange_yoke_inlay_' + str(side), lo, hi,
                       [(-.050, .640), (.090, .640), (.090, .850), (.055, .884), (-.050, .884)], ORANGE)
    if level >= 4:
        axis_lathe(gs['yaw'], 'rear_gold_collar',
                   [(.322 * scale, -.181), (.347 * scale, -.181),
                    (.347 * scale, -.128), (.322 * scale, -.128)], GOLD, hollow=True)
        gs['base'].ring('pedestal_gold_seat', .320, .355, .304, .346, GOLD)
    if level >= 5:
        for side in (-1, 1):
            x, y = side * .405 * scale, -.275
            top = 1.130 if level < 7 else 1.190
            box(gs['yaw'], 'motor_mount_' + str(side), (x, -.150, .695), (.135, .270, .080), DARK)
            gs['yaw'].lathe('motor_lower_cap_' + str(side),
                            [(.087, .660), (.110, .683), (.110, .728), (.096, .746)], GOLD, (x, y))
            gs['yaw'].lathe('motor_bank_' + str(side),
                            [(.086, .734), (.098, .755), (.098, top - .040), (.086, top)], RED, (x, y))
            gs['yaw'].lathe('motor_upper_cap_' + str(side),
                            [(.099, top - .035), (.111, top - .015), (.111, top + .024), (.088, top + .040)], GOLD, (x, y))
            gs['yaw'].ring('motor_waist_strap_' + str(side), .838, .868, .096, .108, GOLD, (x, y))
    if level >= 6:
        remove_groups(rotor, ['wind_blade_'])
        blades = 8 if level == 8 else 6
        width = .70 if blades == 8 else .94
        outline = [(-.028, .055), (.022, .055), (.075, .105), (.150, .170),
                   (.120, .220), (.045, .245), (-.038, .235), (-.045, .180)]
        for blade in range(blades):
            angle = blade * math.tau / blades
            verts = []
            for thickness in (-.012, .012):
                for x, z in outline:
                    x *= width
                    verts.append(((x * math.cos(angle) + z * math.sin(angle)) * scale,
                                  .300 + thickness + .30 * x,
                                  .960 + (-x * math.sin(angle) + z * math.cos(angle)) * scale))
            n = len(outline)
            faces = [tuple(range(n)), tuple(reversed(range(n, 2 * n)))]
            faces += [(i, i + n, (i + 1) % n + n, (i + 1) % n) for i in range(n)]
            gs['rotor'].add('wind_blade_%02d' % blade, verts, faces, STEEL)
        axis_lathe(gs['yaw'], 'rear_motor_extension',
                   [(.170, -.304), (.186, -.325), (.186, -.394), (.155, -.418)], RED)
        axis_lathe(gs['yaw'], 'rear_motor_gold_cap', [(.157, -.418), (.175, -.435), (.175, -.459)], GOLD)
    if level >= 7:
        remove_groups(mount, ['wind_barrel_rib_'])
        for i in range(4):
            a = math.pi / 4 + i * math.pi / 2
            start = len(gs['yaw'].verts)
            box(gs['yaw'], 'gold_duct_rail_%d' % i, (0, .060, .345 * scale), (.052, .420, .036), GOLD)
            for index in range(start, len(gs['yaw'].verts)):
                x, y, z = gs['yaw'].verts[index]
                gs['yaw'].verts[index] = (x * math.cos(a) + z * math.sin(a), y,
                                         .960 - x * math.sin(a) + z * math.cos(a))
        for side in (-1, 1):
            gs['yaw'].ring('motor_upper_strap_' + str(side), 1.030, 1.060, .096, .108,
                           GOLD, (side * .405 * scale, -.275))
        gs['base'].ring('lower_gold_belt', .074, .108, .588, .638, GOLD)
    if level >= 8:
        remove_groups(mount, ['wind_front_rim'])
        axis_lathe(gs['yaw'], 'wind_front_rim',
                   [(.302 * scale, .309), (.390 * scale, .328), (.390 * scale, .403),
                    (.371 * scale, .440), (.266 * scale, .440), (.266 * scale, .309)], GOLD, hollow=True)
        gs['base'].ring('master_red_belt', .185, .225, .589, .626, RED)
        for side in (-1, 1):
            x = side * .405 * scale
            gs['yaw'].lathe('motor_terminal_' + str(side),
                            [(.043, 1.224), (.043, 1.260), (.032, 1.281)], DARK, (x, -.275), 8)
            lo, hi = sorted((side * .454 * scale, side * .470 * scale))
            for i in range(3):
                side_panel(gs['yaw'], 'master_rank_%d_%d' % (side, i), lo, hi,
                           [(-.020, .685 + i * .050), (.050, .685 + i * .050),
                            (.050, .702 + i * .050), (-.020, .702 + i * .050)], GOLD)
    for key, obj in (('base', base), ('yaw', mount), ('rotor', rotor)):
        append_geometry(obj, gs[key], collection, material)
        obj.data.update()
    objects['socket_ui_top'].location.z = 1.550
    # Root-level collision is deliberately stable across upgrades.
    if level > 1:
        collider = objects['COL_BOX_BODY']
        for v in collider.data.vertices:
            v.co.x = math.copysign(.680, v.co.x)
            v.co.y = math.copysign(.680, v.co.y)
            v.co.z = 0 if v.co.z < .01 else 1.470


def clean_export_names(path):
    raw = path.read_bytes()
    length = struct.unpack_from('<I', raw, 12)[0]
    doc = json.loads(raw[20:20 + length])
    for node in doc['nodes']:
        node['name'] = node.get('extras', {}).get('canonical_name', node['name'])
        if 'mesh' in node:
            doc['meshes'][node['mesh']]['name'] = node['name']
    data = json.dumps(doc, separators=(',', ':')).encode()
    data += b' ' * (-len(data) % 4)
    tail = raw[20 + length:]
    path.write_bytes(struct.pack('<III', 0x46546c67, 2, 20 + len(data) + len(tail)) +
                     struct.pack('<II', len(data), 0x4e4f534a) + data + tail)


def build():
    assert not bpy.data.scenes.get('WIND_BLOWER_L01'), 'Use a fresh process, or preserve the edited level scenes first.'
    original_hash = hashlib.sha256(BASE.read_bytes()).hexdigest()
    if bpy.data.filepath and Path(bpy.data.filepath).resolve() == SOURCE.resolve():
        original = bpy.data.scenes['WIND_BLOWER']
    else:
        with bpy.data.libraries.load(str(SOURCE), link=False) as (available, loaded):
            loaded.scenes = ['WIND_BLOWER']
        original = loaded.scenes[0]
    source_objects = [o for o in original.objects if o.type not in ('LIGHT', 'CAMERA')]
    for level in range(1, 9):
        scene = bpy.data.scenes.new('WIND_BLOWER_L%02d' % level)
        bpy.context.window.scene = scene
        scene.unit_settings.system = 'METRIC'
        scene.unit_settings.scale_length = 1
        model = bpy.data.collections.new(scene.name + '_MODEL')
        collision = bpy.data.collections.new(scene.name + '_COLLISION')
        scene.collection.children.link(model)
        scene.collection.children.link(collision)
        copies = {}
        for source in source_objects:
            obj = source.copy()
            canonical = source.name.split('.')[0]
            obj.name = 'L%02d_%s' % (level, canonical)
            obj['canonical_name'] = canonical
            if source.type == 'MESH':
                obj.data = source.data.copy()
            (collision if canonical.startswith('COL_') else model).objects.link(obj)
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
        output = OUT / ('wind_blower_lvl%02d.glb' % level)
        bpy.ops.export_scene.gltf(filepath=str(output), use_selection=True, use_active_scene=True,
                                 use_visible=False, use_renderable=False, export_yup=True,
                                 export_normals=True, export_texcoords=True, export_materials='EXPORT',
                                 export_animations=False, export_extras=True)
        clean_export_names(output)
        studio(scene)
        scenes[level] = scene
    assert hashlib.sha256(BASE.read_bytes()).hexdigest() == original_hash
    bpy.context.window.scene = scenes[8]
    bpy.data.libraries.write(str(ART / 'wind_blower_levels.blend'), set(scenes.values()), fake_user=True)
    print('WIND_LEVELS_BUILT', original_hash, flush=True)


def render(level, view='beauty'):
    scene = bpy.data.scenes['WIND_BLOWER_L%02d' % level]
    bpy.context.window.scene = scene
    pos, target, scale = {
        'beauty': ((2.6, 4.5, 2.6), (0, 0, .73), 2.15),
        'front': ((0, 5, .74), (0, 0, .74), 1.95),
        'side': ((5, 0, .74), (0, 0, .74), 1.95),
        'back': ((0, -5, .74), (0, 0, .74), 1.95),
        'top': ((0, .001, 5), (0, 0, 0), 1.75),
    }[view]
    scene.camera.location = pos
    aim(scene.camera, target)
    scene.camera.data.ortho_scale = scale
    scene.render.filepath = str(ART / ('level_%d_%s.png' % (level, view)))
    bpy.ops.render.render(write_still=True)


def overview():
    scene = bpy.data.scenes.new('WIND_BLOWER_LEVEL_OVERVIEW')
    bpy.context.window.scene = scene
    studio(scene)
    for level in range(1, 9):
        source = bpy.data.scenes['WIND_BLOWER_L%02d' % level]
        offset = Vector(((1.5 - (level - 1) % 4) * 1.75, 0, (1 - (level - 1) // 4) * 1.9))
        for obj in source.objects:
            if obj.type == 'MESH' and not obj.name.startswith('L%02d_COL_' % level):
                copy = bpy.data.objects.new('preview_' + obj.name, obj.data)
                scene.collection.objects.link(copy)
                copy.matrix_world = Matrix.Translation(offset) @ obj.matrix_world
        label = bpy.data.curves.new('LEVEL_%d' % level, 'FONT')
        label.body, label.align_x, label.size = 'LEVEL %d' % level, 'CENTER', .13
        obj = bpy.data.objects.new(label.name, label)
        scene.collection.objects.link(obj)
        obj.location = (offset.x, .68, offset.z - .12)
        obj.rotation_euler = (math.pi / 2, 0, math.pi)
    scene.camera.location = (1.4, 13, 5.3)
    aim(scene.camera, (0, 0, 1.55))
    scene.camera.data.ortho_scale = 7.5
    scene.render.resolution_x, scene.render.resolution_y = 1800, 1100
    scene.render.filepath = str(ART / 'levels_1-8.png')
    bpy.ops.render.render(write_still=True)
    scene = bpy.data.scenes.new('WIND_BLOWER_FAMILY_COMPARISON')
    bpy.context.window.scene = scene
    studio(scene)
    geometry.import_visible(ROOT / 'assets/models/buildings/cannon.glb',
                            lambda o: 'upgrade_' not in o.name or '_l8_' in o.name, -2.4)
    geometry.import_visible(OUT / 'wind_blower_lvl01.glb', offset=-.8)
    geometry.import_visible(OUT / 'wind_blower_lvl08.glb', offset=.8)
    geometry.import_visible(ROOT / 'assets/models/buildings/flamethrower.glb', offset=2.4)
    geometry.import_visible(ROOT / 'assets/models/buildings/flamethrower_upgrades.glb',
                            lambda o: '_l8_' in o.name, 2.4)
    scene.camera.location = (1, 11, 4.6)
    aim(scene.camera, (0, 0, .72))
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

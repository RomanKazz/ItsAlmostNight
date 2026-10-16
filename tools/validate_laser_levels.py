"""Raw GLB, fresh import and complete aiming sweep for all laser variants."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import struct

import bpy
import bmesh
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/laser_levels'
OUT = ROOT / 'assets/models/buildings/laser_levels'
spec = importlib.util.spec_from_file_location('laser_levels_authoring', ROOT / 'tools/create_laser_levels.py')
authoring = importlib.util.module_from_spec(spec)
spec.loader.exec_module(authoring)
spec = importlib.util.spec_from_file_location('laser_levels_checks', ROOT / 'tools/validate_wind_blower_levels.py')
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)
PALETTE = check.PALETTE


def raw_asset(level):
    path = OUT / ('laser_lvl%02d.glb' % level)
    blob = path.read_bytes()
    assert struct.unpack_from('<III', blob) == (0x46546c67, 2, len(blob))
    length, kind = struct.unpack_from('<II', blob, 12)
    assert kind == 0x4e4f534a
    doc, binary = json.loads(blob[20:20 + length]), blob[28 + length:]
    assert not doc.get('extensionsRequired') and not doc.get('animations') and not doc.get('skins')
    assert len(doc['scenes']) == 1 and not doc.get('cameras')
    assert all('uri' not in b for b in doc['buffers'])
    assert len(doc['images']) == 1 and 'uri' not in doc['images'][0]
    view = doc['bufferViews'][doc['images'][0]['bufferView']]
    start = view.get('byteOffset', 0)
    assert binary[start:start + view['byteLength']] == PALETTE

    def values(index):
        a = doc['accessors'][index]
        v = doc['bufferViews'][a['bufferView']]
        component = {5126: 'f', 5125: 'I', 5123: 'H', 5121: 'B'}[a['componentType']]
        width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
        row = '<' + component * width
        offset = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        stride = v.get('byteStride', struct.calcsize(row))
        return [struct.unpack_from(row, binary, offset + i * stride) for i in range(a['count'])]

    nodes = doc['nodes']
    byname = {n['name']: i for i, n in enumerate(nodes)}
    expected = {'asset_root', 'base_mesh', 'yaw_mesh', 'pitch_mesh', 'energy_mesh', 'COL_BOX_BODY',
                'yaw_pivot', 'pitch_pivot', 'MUZZLE_01', 'socket_base', 'socket_ui_top', 'socket_effect_energy'}
    assert len(byname) == len(nodes) and set(byname) == expected
    parents, world = {}, {}

    def visit(i, parent=None):
        parents[i] = parent
        world[i] = (world[parent] if parent is not None else Matrix.Identity(4)) @ check.node_transform(nodes[i])
        for j in nodes[i].get('children', []):
            visit(j, i)

    root = byname['asset_root']
    assert doc['scenes'][0]['nodes'] == [root]
    visit(root)
    assert check.node_transform(nodes[root]) == Matrix.Identity(4)
    transforms = {}
    for i, node in enumerate(nodes):
        check.close(node.get('scale', [1, 1, 1]), (1, 1, 1))
        assert max(abs(c) for row in check.node_transform(node).to_3x3() - Matrix.Identity(3) for c in row) < 1e-5
        if i != root:
            parent = {'yaw_mesh': 'yaw_pivot', 'pitch_pivot': 'yaw_pivot', 'pitch_mesh': 'pitch_pivot',
                      'energy_mesh': 'pitch_pivot', 'MUZZLE_01': 'pitch_pivot',
                      'socket_effect_energy': 'pitch_pivot'}.get(node['name'], 'asset_root')
            assert parents[i] == byname[parent]
        transforms[node['name']] = {'parent': None if i == root else nodes[parents[i]]['name'],
            'translation_local': node.get('translation', [0, 0, 0]),
            'rotation_xyzw': node.get('rotation', [0, 0, 0, 1]), 'scale': node.get('scale', [1, 1, 1]),
            'position_relative_to_root': list(world[i].translation)}
    for name, position in {'yaw_pivot': (0, .640, 0), 'pitch_pivot': (0, 1.020, .170),
                           'MUZZLE_01': (0, 1.020, -.459), 'socket_effect_energy': (0, 1.020, .385),
                           'socket_ui_top': (0, 1.550, 0)}.items():
        check.close(world[byname[name]].translation, position)
    check.close(nodes[byname['MUZZLE_01']].get('translation'), (0, 0, -.629))
    check.close(world[byname['MUZZLE_01']].to_3x3() @ Vector((0, 0, -1)), (0, 0, -1))
    check.close(world[byname['MUZZLE_01']].to_3x3() @ Vector((0, 1, 0)), (0, 1, 0))
    visible, collision, counts, order, materials = [], [], {}, {}, set()
    for i, node in enumerate(nodes):
        if 'mesh' not in node:
            continue
        primitives = doc['meshes'][node['mesh']]['primitives']
        assert len(primitives) == 1
        p = primitives[0]
        assert p.get('mode', 4) == 4
        attrs = p['attributes']
        positions, normals = values(attrs['POSITION']), values(attrs['NORMAL'])
        indices = [v[0] for v in values(p['indices'])]
        assert all(all(math.isfinite(c) for c in v) for v in positions)
        assert all(abs(Vector(n).length - 1) < 1e-4 for n in normals)
        faces = set()
        for start in range(0, len(indices), 3):
            tri = [Vector(positions[j]) for j in indices[start:start + 3]]
            normal = (tri[1] - tri[0]).cross(tri[2] - tri[0])
            assert normal.length > 1e-9, (level, node['name'], 'degenerate')
            assert all(normal.dot(Vector(normals[j])) > 0 for j in indices[start:start + 3]), (level, node['name'], 'normal')
            key = tuple(sorted(tuple(round(c, 6) for c in v) for v in tri))
            assert key not in faces, (level, node['name'], 'duplicate')
            faces.add(key)
        counts[node['name']] = len(indices) // 3
        points = [world[i] @ Vector(v) for v in positions]
        if node['name'].startswith('COL_'):
            collision.extend(points)
            assert len(set(positions)) == 8 and len(indices) == 36
        else:
            visible.extend(points)
            order[node['name']] = len(order)
            materials.add(p['material'])
            for u, v in values(attrs['TEXCOORD_0']):
                assert min(abs(u - c) for c in (.09375, .84375, .59375, .46875, .03125, .65625, .96875)) < 1e-5
                assert .025 - 1e-5 <= 1 - v <= .225 + 1e-5 or .275 - 1e-5 <= 1 - v <= .475 + 1e-5
    assert order == {'base_mesh': 0, 'energy_mesh': 1, 'pitch_mesh': 2, 'yaw_mesh': 3}
    assert len(doc['meshes']) == 5 and len(materials) == 2 and len(doc['materials']) == 3
    for index in materials:
        pbr = doc['materials'][index]['pbrMetallicRoughness']
        assert pbr.get('metallicFactor', 1) == 0 and pbr.get('roughnessFactor', 1) == 1
    energy = doc['materials'][doc['meshes'][nodes[byname['energy_mesh']]['mesh']]['primitives'][0]['material']]
    check.close(energy['emissiveFactor'], (.65, .65, .65))
    vb, cb = check.bounds(visible), check.bounds(collision)
    check.close(cb['min'], (-.600, 0, -.600) if level == 1 else (-.700, 0, -.700))
    check.close(cb['max'], (.600, 1.400, .600) if level == 1 else (.700, 1.500, .700))
    assert abs(vb['min'][1]) < 1e-6
    assert all(cb['min'][i] <= vb['min'][i] + 1e-6 and cb['max'][i] >= vb['max'][i] - 1e-6 for i in range(3))
    return {'level': level, 'description': authoring.DESCRIPTIONS[level - 1], 'file': str(path.relative_to(ROOT)),
            'sha256': hashlib.sha256(blob).hexdigest(), 'visible_bounds_glb': vb, 'collision_bounds_glb': cb,
            'visible_triangles': sum(v for k, v in counts.items() if not k.startswith('COL_')),
            'visible_meshes': 4, 'visible_primitives': 4, 'visible_materials': 2,
            'triangles_by_mesh': counts, 'render_order': order, 'transforms_glb': transforms,
            'raw_glb_verified': True, 'embedded_palette_identical': True}


def motion(level):
    scene = bpy.data.scenes['LASER_L%02d' % level]
    bpy.context.window.scene = scene
    objects = {o['canonical_name']: o for o in scene.objects if o.get('canonical_name')}
    base, yaw, head, energy, spin, pitch = [objects[k] for k in
        ('base_mesh', 'yaw_mesh', 'pitch_mesh', 'energy_mesh', 'yaw_pivot', 'pitch_pivot')]
    spin.rotation_mode = pitch.rotation_mode = 'XYZ'
    bpy.context.view_layer.update()
    image = next(n.image for n in base.data.materials[0].node_tree.nodes if n.type == 'TEX_IMAGE')
    assert image.packed_file and bytes(image.packed_file.data) == PALETTE
    # Preserve the exact original lens, crystal and muzzle geometry on every level.
    reference = next(o for o in bpy.data.scenes['LASER_L01'].objects if o.get('canonical_name') == 'energy_mesh')
    for group in ('laser_front_lens', 'laser_reactor_crystal'):
        original = [reference.data.vertices[j].co for j in sorted(check.ids(reference, group))]
        current = [energy.data.vertices[j].co for j in sorted(check.ids(energy, group))]
        assert len(original) == len(current) and all((a - b).length < 1e-6 for a, b in zip(original, current))
    base_tree = check.tree(base)
    poses = sorted(set((a, p) for a in range(360) for p in (-12, 0, 20)) |
                   set((a, p) for a in range(0, 360, 45) for p in range(-12, 21)))
    overlaps = []
    height, radius = 0, 0
    for angle, elevation in poses:
        spin.rotation_euler.z, pitch.rotation_euler.x = math.radians(angle), math.radians(elevation)
        bpy.context.view_layer.update()
        mount_tree = check.tree(yaw)
        for obj, exclude in ((head, ('laser_trunnion_axle',)), (energy, ())):
            moving = check.tree(obj, exclude=exclude)
            if moving.overlap(mount_tree) or moving.overlap(base_tree):
                overlaps.append([angle, elevation, obj.name])
        for obj in (yaw, head, energy):
            for vertex in obj.data.vertices:
                point = obj.matrix_world @ vertex.co
                height = max(height, point.z)
                radius = max(radius, math.hypot(point.x, point.y))
        forward = objects['MUZZLE_01'].matrix_world.to_3x3() @ Vector((0, 1, 0))
        assert abs(forward.z - math.sin(math.radians(elevation))) < 1e-5
    spin.rotation_euler.z = pitch.rotation_euler.x = 0
    bpy.context.view_layer.update()
    print('LASER_MOTION', level, 'overlaps', overlaps[:5], 'height', height, 'radius', radius, flush=True)
    assert not overlaps, ('aiming collision', level, overlaps[:10])
    assert height < (1.400 if level == 1 else 1.500) and radius < (.600 if level == 1 else .700)
    return {'poses': len(poses), 'yaw_step_degrees': 1, 'pitch_step_degrees': 1,
            'unintended_intersections': 0, 'maximum_moving_height': height,
            'maximum_moving_radius': radius, 'original_lens_crystal_and_muzzle': True}


def round_trip(level):
    scene = bpy.data.scenes.new('LASER_LEVEL_ROUNDTRIP')
    bpy.context.window.scene = scene
    bpy.ops.import_scene.gltf(filepath=str(OUT / ('laser_lvl%02d.glb' % level)))
    bpy.context.view_layer.update()
    points = []
    objects = {o.name.split('.')[0]: o for o in scene.objects}
    check.close(objects['MUZZLE_01'].matrix_world.translation, (0, .459, 1.020))
    check.close(objects['pitch_pivot'].matrix_world.translation, (0, -.170, 1.020))
    for obj in scene.objects:
        if obj.type != 'MESH':
            continue
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-6)
        assert all(e.is_manifold and e.is_contiguous for e in bm.edges), (level, obj.name)
        assert bm.calc_volume(signed=True) > 0
        bm.free()
        if not obj.name.startswith('COL_'):
            points.extend(obj.matrix_world @ v.co for v in obj.data.vertices)
    tree = KDTree(len(points))
    for i, point in enumerate(points):
        tree.insert(point, i)
    tree.balance()
    # Original left/right pivot bolts have the same local axial profile: their
    # cap endpoints differ by 3 mm under reflection. Preserve that source detail.
    deviation = max(tree.find(Vector((-p.x, p.y, p.z)))[2] for p in points)
    assert deviation < .00301, (level, 'symmetry', deviation)
    for obj in list(scene.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.scenes.remove(scene)
    return deviation


def main():
    assert bpy.app.background
    bpy.ops.wm.open_mainfile(filepath=str(ART / 'laser_levels.blend'))
    rows = []
    for level in range(1, 9):
        row = raw_asset(level)
        row['motion'] = motion(level)
        row['maximum_mirror_vertex_deviation_m'] = round_trip(level)
        row['fresh_import_verified'] = row['normals_uv_manifold_verified'] = True
        rows.append(row)
        print('LASER_LEVEL_VALIDATED', level, row['visible_triangles'], flush=True)
    bpy.context.window.scene = bpy.data.scenes['LASER_L08']
    for other in list(bpy.data.scenes):
        if not other.name.startswith('LASER_L'):
            bpy.data.scenes.remove(other)
    for scene in bpy.data.scenes:
        scene.use_fake_user = True
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'laser_levels.blend'))
    manifest = {'asset': 'laser_levels', 'levels': rows,
        'mode': 'Eight complete replacement models; draw one level, not additive overlays',
        'original_base_sha256': hashlib.sha256(authoring.BASE.read_bytes()).hexdigest(),
        'source_blend_sha256': hashlib.sha256(authoring.SOURCE.read_bytes()).hexdigest(),
        'palette': {'path': 'assets/models/buildings/Textures/colormap.png',
                    'sha256': hashlib.sha256(PALETTE).hexdigest(), 'embedded_glb': True, 'packed_blend': True},
        'coordinates': 'GLB +Y up, -Z forward; Blender +Z up, +Y forward; metres',
        'footprint_m': [2, 2], 'motion': 'Full yaw; pitch -12 to +20 degrees; no recoil',
        'license': 'Kenney Tower Defense Kit 2.1 palette CC0; original project geometry',
        'integration': {'gameplay_changed': False, 'renderer_connected': False,
                        'loader_verified': False, 'playtested': False}}
    (ART / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print('LASER_ALL_LEVELS_VERIFIED', flush=True)


if __name__ == '__main__':
    main()

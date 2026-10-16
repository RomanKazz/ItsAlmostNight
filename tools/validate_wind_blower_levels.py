"""Validate the eight new GLBs and the editable rig; do not edit gameplay files."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import struct

import bpy
import bmesh
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/wind_blower_levels'
OUT = ROOT / 'assets/models/buildings/wind_blower_levels'
PALETTE = (ROOT / 'assets/models/buildings/Textures/colormap.png').read_bytes()
spec = importlib.util.spec_from_file_location('wind_levels_validation_data', ROOT / 'tools/create_wind_blower_levels.py')
authoring = importlib.util.module_from_spec(spec)
spec.loader.exec_module(authoring)


def close(a, b):
    assert (Vector(a) - Vector(b)).length < 1e-5, (tuple(a), tuple(b))


def bounds(points):
    lo = [min(v[i] for v in points) for i in range(3)]
    hi = [max(v[i] for v in points) for i in range(3)]
    return {'min': lo, 'max': hi, 'size': [hi[i] - lo[i] for i in range(3)]}


def node_transform(node):
    if 'matrix' in node:
        return Matrix([node['matrix'][i::4] for i in range(4)])
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    return Matrix.LocRotScale(Vector(node.get('translation', [0, 0, 0])), Quaternion((w, x, y, z)),
                              Vector(node.get('scale', [1, 1, 1])))


def raw_asset(level):
    path = OUT / ('wind_blower_lvl%02d.glb' % level)
    blob = path.read_bytes()
    assert struct.unpack_from('<III', blob) == (0x46546c67, 2, len(blob))
    length, kind = struct.unpack_from('<II', blob, 12)
    assert kind == 0x4e4f534a
    doc = json.loads(blob[20:20 + length])
    binary = blob[28 + length:]
    assert not doc.get('extensionsRequired') and not doc.get('animations') and not doc.get('skins')
    assert len(doc['scenes']) == 1 and not doc.get('cameras')
    assert all('uri' not in b for b in doc['buffers'])
    assert len(doc['images']) == 1 and 'uri' not in doc['images'][0]
    view = doc['bufferViews'][doc['images'][0]['bufferView']]
    offset = view.get('byteOffset', 0)
    assert binary[offset:offset + view['byteLength']] == PALETTE

    def values(index):
        accessor = doc['accessors'][index]
        view = doc['bufferViews'][accessor['bufferView']]
        component = {5126: 'f', 5125: 'I', 5123: 'H', 5121: 'B'}[accessor['componentType']]
        width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[accessor['type']]
        row = '<' + component * width
        offset = view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
        stride = view.get('byteStride', struct.calcsize(row))
        return [struct.unpack_from(row, binary, offset + i * stride) for i in range(accessor['count'])]

    nodes = doc['nodes']
    byname = {n['name']: i for i, n in enumerate(nodes)}
    expected = {'asset_root', 'base_mesh', 'yaw_mesh', 'rotor_mesh', 'COL_BOX_BODY',
                'yaw_pivot', 'rotor_pivot', 'MUZZLE_01', 'socket_base', 'socket_ui_top'}
    assert len(byname) == len(nodes) and set(byname) == expected
    world, parents = {}, {}

    def visit(i, parent=None):
        parents[i] = parent
        world[i] = (world[parent] if parent is not None else Matrix.Identity(4)) @ node_transform(nodes[i])
        for child in nodes[i].get('children', []):
            visit(child, i)

    visit(byname['asset_root'])
    for i, node in enumerate(nodes):
        close(node.get('scale', [1, 1, 1]), (1, 1, 1))
        assert max(abs(c) for row in node_transform(node).to_3x3() - Matrix.Identity(3) for c in row) < 1e-5
        if node['name'] != 'asset_root':
            parent = {'yaw_mesh': 'yaw_pivot', 'rotor_mesh': 'rotor_pivot',
                      'rotor_pivot': 'yaw_pivot', 'MUZZLE_01': 'yaw_pivot'}.get(node['name'], 'asset_root')
            assert parents[i] == byname[parent]
    close(world[byname['asset_root']].translation, (0, 0, 0))
    close(world[byname['yaw_pivot']].translation, (0, .350, 0))
    close(world[byname['rotor_pivot']].translation, (0, .960, -.300))
    close(world[byname['MUZZLE_01']].translation, (0, .960, -.479))
    close(world[byname['MUZZLE_01']].to_3x3() @ Vector((0, 0, -1)), (0, 0, -1))
    close(world[byname['MUZZLE_01']].to_3x3() @ Vector((0, 1, 0)), (0, 1, 0))
    close(world[byname['socket_ui_top']].translation, (0, 1.550, 0))
    visible, collision = [], []
    counts, render_order, transforms = {}, {}, {}
    materials = set()
    for i, node in enumerate(nodes):
        transforms[node['name']] = {
            'parent': nodes[parents[i]]['name'] if parents[i] is not None else None,
            'translation_local': node.get('translation', [0, 0, 0]),
            'rotation_xyzw': node.get('rotation', [0, 0, 0, 1]),
            'scale': node.get('scale', [1, 1, 1]),
            'position_relative_to_root': list(world[i].translation),
        }
        if 'mesh' not in node:
            continue
        primitives = doc['meshes'][node['mesh']]['primitives']
        assert len(primitives) == 1
        primitive = primitives[0]
        attrs = primitive['attributes']
        positions, normals = values(attrs['POSITION']), values(attrs['NORMAL'])
        indices = [row[0] for row in values(primitive['indices'])]
        assert all(all(math.isfinite(c) for c in v) for v in positions)
        assert all(abs(Vector(n).length - 1) < 1e-4 for n in normals)
        triangles = set()
        for start in range(0, len(indices), 3):
            vertices = [Vector(positions[j]) for j in indices[start:start + 3]]
            normal = (vertices[1] - vertices[0]).cross(vertices[2] - vertices[0])
            assert normal.length > 1e-9
            assert all(normal.dot(Vector(normals[j])) > 0 for j in indices[start:start + 3]), node['name']
            key = tuple(sorted(tuple(round(c, 6) for c in v) for v in vertices))
            assert key not in triangles, ('duplicate face', node['name'])
            triangles.add(key)
        points = [world[i] @ Vector(v) for v in positions]
        counts[node['name']] = len(indices) // 3
        if node['name'].startswith('COL_'):
            collision.extend(points)
            assert len(set(positions)) == 8 and len(indices) == 36
        else:
            visible.extend(points)
            render_order[node['name']] = len(render_order)
            materials.add(primitive['material'])
            assert 'TEXCOORD_0' in attrs
            for u, v in values(attrs['TEXCOORD_0']):
                assert min(abs(u - column) for column in (.09375, .84375, .59375, .46875, .65625, .96875)) < 1e-5
                assert .025 - 1e-5 <= 1 - v <= .225 + 1e-5 or .275 - 1e-5 <= 1 - v <= .475 + 1e-5
    assert render_order == {'base_mesh': 0, 'rotor_mesh': 1, 'yaw_mesh': 2}
    assert len(doc['meshes']) == 4 and len(materials) == 1 and len(doc['materials']) == 2
    for material in materials:
        pbr = doc['materials'][material]['pbrMetallicRoughness']
        assert pbr.get('metallicFactor', 1) == 0 and pbr.get('roughnessFactor', 1) == 1
    cb = bounds(collision)
    close(cb['min'], (-.610, 0, -.610) if level == 1 else (-.680, 0, -.680))
    close(cb['max'], (.610, 1.340, .610) if level == 1 else (.680, 1.470, .680))
    return {'level': level, 'description': authoring.DESCRIPTIONS[level - 1],
            'file': str(path.relative_to(ROOT)), 'sha256': hashlib.sha256(blob).hexdigest(),
            'visible_bounds_glb': bounds(visible), 'collision_bounds_glb': cb,
            'visible_triangles': sum(v for k, v in counts.items() if not k.startswith('COL_')),
            'visible_meshes': 3, 'visible_primitives': 3, 'visible_materials': 1,
            'triangles_by_mesh': counts, 'render_order': render_order, 'transforms_glb': transforms}


def ids(obj, prefix):
    return authoring.group_vertices(obj, prefix)


def tree(obj, only=None, exclude=()):
    obj.data.calc_loop_triangles()
    chosen = ids(obj, only) if only is not None else None
    omitted = set().union(*(ids(obj, prefix) for prefix in exclude)) if exclude else set()
    tris = [tuple(t.vertices) for t in obj.data.loop_triangles
            if (chosen is None or all(i in chosen for i in t.vertices)) and not all(i in omitted for i in t.vertices)]
    return BVHTree.FromPolygons([obj.matrix_world @ v.co for v in obj.data.vertices], tris, all_triangles=True)


def motion(level):
    scene = bpy.data.scenes['WIND_BLOWER_L%02d' % level]
    bpy.context.window.scene = scene
    objects = {o['canonical_name']: o for o in scene.objects if o.get('canonical_name')}
    base, mount, rotor, yaw, spin = [objects[k] for k in ('base_mesh', 'yaw_mesh', 'rotor_mesh', 'yaw_pivot', 'rotor_pivot')]
    yaw.rotation_mode = spin.rotation_mode = 'XYZ'
    bpy.context.view_layer.update()
    image = next(n.image for n in base.data.materials[0].node_tree.nodes if n.type == 'TEX_IMAGE')
    assert image.packed_file and bytes(image.packed_file.data) == PALETTE
    # Yoke bottom corners must remain supported even as the head widens.
    seat = tree(mount, only='wind_yoke_seat')
    yoke_points = [mount.matrix_world @ mount.data.vertices[i].co for i in ids(mount, 'wind_u_yoke')]
    bottom = min(v.z for v in yoke_points)
    for point in yoke_points:
        if abs(point.z - bottom) < 1e-5:
            hit, *_ = seat.ray_cast(Vector((point.x, point.y, .600)), Vector((0, 0, -1)), .200)
            assert hit is not None and hit.z >= point.z - 1e-6, ('floating yoke', level)
    blade_count = 8 if level == 8 else 6 if level >= 6 else 4
    for i in range(blade_count):
        for j in range(i + 1, blade_count):
            assert not tree(rotor, only='wind_blade_%02d' % i).overlap(
                tree(rotor, only='wind_blade_%02d' % j)), ('blades overlap', level, i, j)
    tip = max(math.hypot(rotor.data.vertices[i].co.x, rotor.data.vertices[i].co.z) for i in ids(rotor, 'wind_blade_'))
    inradius = .262 * authoring.SCALES[level - 1] * math.cos(math.pi / 12)
    assert tip < inradius, ('blade containment', level, tip, inradius)
    base_tree = tree(base)
    poses = sorted(set((a, p) for a in range(360) for p in (0, 30, 60)) |
                   set((a, p) for a in range(0, 360, 45) for p in range(360)))
    max_radius, max_height = 0, 0
    overlaps = []
    for angle, phase in poses:
        yaw.rotation_euler.z = math.radians(angle)
        spin.rotation_euler.y = math.radians(phase)
        bpy.context.view_layer.update()
        moving = tree(rotor, exclude=('wind_rotor_shaft',))
        if moving.overlap(tree(mount)) or moving.overlap(base_tree):
            overlaps.append([angle, phase, 'rotor'])
        if tree(mount, exclude=('wind_turntable',)).overlap(base_tree):
            overlaps.append([angle, phase, 'yaw'])
        for obj in (mount, rotor):
            for v in obj.data.vertices:
                point = obj.matrix_world @ v.co
                max_radius = max(max_radius, math.hypot(point.x, point.y))
                max_height = max(max_height, point.z)
    yaw.rotation_euler.z = spin.rotation_euler.y = 0
    bpy.context.view_layer.update()
    print('LEVEL_MOTION', level, 'poses', len(poses), 'overlaps', overlaps[:5], flush=True)
    assert not overlaps, ('moving parts clip', level, overlaps[:10])
    limit = .610 if level == 1 else .680
    assert max_radius < limit and max_height < (1.340 if level == 1 else 1.470)
    return {'poses': len(poses), 'yaw_step_degrees': 1, 'rotor_step_degrees': 1,
            'unintended_intersections': 0, 'blade_count': blade_count, 'blade_tip_radius': tip,
            'bore_inradius': inradius, 'maximum_moving_radius': max_radius, 'maximum_moving_height': max_height}


def round_trip(level):
    scene = bpy.data.scenes.new('WIND_LEVEL_ROUNDTRIP')
    bpy.context.window.scene = scene
    bpy.ops.import_scene.gltf(filepath=str(OUT / ('wind_blower_lvl%02d.glb' % level)))
    bpy.context.view_layer.update()
    objects = {o.name.split('.')[0]: o for o in scene.objects}
    for obj in scene.objects:
        if obj.type != 'MESH':
            continue
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-6)
        assert all(e.is_manifold and e.is_contiguous for e in bm.edges), (level, obj.name, 'manifold/winding')
        assert bm.calc_volume(signed=True) > 0, (level, obj.name, 'volume')
        bm.free()
    close(objects['MUZZLE_01'].matrix_world.translation, (0, .479, .960))
    close(objects['rotor_pivot'].matrix_world.translation, (0, .300, .960))
    for obj in list(scene.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.scenes.remove(scene)


def main():
    assert bpy.app.background, 'Validation/finalization belongs in a separate Blender process.'
    bpy.ops.wm.open_mainfile(filepath=str(ART / 'wind_blower_levels.blend'))
    rows = []
    for level in range(1, 9):
        row = raw_asset(level)
        row['motion'] = motion(level)
        round_trip(level)
        row['raw_glb_verified'] = row['fresh_import_verified'] = row['normals_uv_manifold_verified'] = True
        rows.append(row)
    bpy.context.window.scene = bpy.data.scenes['WIND_BLOWER_L08']
    for other in list(bpy.data.scenes):
        if not other.name.startswith('WIND_BLOWER_L'):
            bpy.data.scenes.remove(other)
    for scene in bpy.data.scenes:
        scene.use_fake_user = True
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_perspective = 'CAMERA'
            area.spaces.active.shading.type = 'MATERIAL'
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'wind_blower_levels.blend'))
    manifest = {'asset': 'wind_blower_levels', 'levels': rows,
                'mode': 'Eight complete standalone variants; choose one, never combine them as overlays',
                'original_base_sha256': hashlib.sha256(authoring.BASE.read_bytes()).hexdigest(),
                'source_blend_sha256': hashlib.sha256(authoring.SOURCE.read_bytes()).hexdigest(),
                'palette': {'path': 'assets/models/buildings/Textures/colormap.png',
                            'sha256': hashlib.sha256(PALETTE).hexdigest(), 'embedded_glb': True, 'packed_blend': True},
                'coordinates': 'GLB +Y up, -Z forward; Blender +Z up, +Y forward; metres',
                'footprint_m': [2, 2], 'pitch_degrees': [0, 0], 'recoil_m': 0,
                'rig': {'yaw_axis_glb': [0, 1, 0], 'yaw_pivot_glb': [0, .350, 0],
                        'rotor_axis_glb': [0, 0, -1], 'rotor_pivot_glb': [0, .960, -.300],
                        'muzzle_glb': [0, .960, -.479], 'muzzle_parent': 'yaw_pivot'},
                'intentional_contacts': ['Turntable seats on the pedestal', 'Rotor shaft is inside its bearing',
                                         'Motor-bank support brackets are fixed to the yoke', 'Blade roots sit inside the hub'],
                'license': 'Kenney Tower Defense Kit 2.1 colormap CC0; original project geometry',
                'integration': {'gameplay_changed': False, 'renderer_connected': True,
                                'loader_verified': False, 'playtested': False}}
    (ART / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('WIND_LEVELS_VALIDATED', [(r['level'], r['visible_triangles']) for r in rows], flush=True)


if __name__ == '__main__':
    main()

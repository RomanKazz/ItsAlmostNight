"""Raw GLB, fresh-import and mechanical checks for the eight static beacons."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import struct

import bpy
import bmesh
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/decoy_beacon_levels'
OUT = ROOT / 'assets/models/buildings/decoy_beacon_levels'
spec = importlib.util.spec_from_file_location('decoy_levels_authoring', ROOT / 'tools/create_decoy_beacon_levels.py')
authoring = importlib.util.module_from_spec(spec)
spec.loader.exec_module(authoring)
spec = importlib.util.spec_from_file_location('decoy_levels_validation_helpers', ROOT / 'tools/validate_wind_blower_levels.py')
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)
PALETTE = check.PALETTE


def raw_asset(level):
    path = OUT / ('decoy_beacon_lvl%02d.glb' % level)
    blob = path.read_bytes()
    assert struct.unpack_from('<III', blob) == (0x46546c67, 2, len(blob))
    length = struct.unpack_from('<I', blob, 12)[0]
    doc, binary = json.loads(blob[20:20 + length]), blob[28 + length:]
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
    expected = {'asset_root', 'base_mesh', 'light_mesh', 'COL_BOX_BODY', 'socket_base',
                'socket_ui_top', 'socket_effect_attract', *('socket_light_%02d' % i for i in range(1, 5))}
    assert len(byname) == len(nodes) and set(byname) == expected
    root = byname['asset_root']
    assert set(nodes[root]['children']) == set(range(len(nodes))) - {root}
    assert check.node_transform(nodes[root]) == Matrix.Identity(4)
    transforms = {}
    for node in nodes:
        check.close(node.get('scale', [1, 1, 1]), (1, 1, 1))
        assert max(abs(c) for row in check.node_transform(node).to_3x3() - Matrix.Identity(3) for c in row) < 1e-5
        transforms[node['name']] = {'parent': None if node['name'] == 'asset_root' else 'asset_root',
            'translation_local': node.get('translation', [0, 0, 0]),
            'rotation_xyzw': node.get('rotation', [0, 0, 0, 1]), 'scale': node.get('scale', [1, 1, 1])}
    check.close(nodes[byname['socket_ui_top']].get('translation'), (0, 1.620, 0))
    check.close(nodes[byname['socket_effect_attract']].get('translation'), (0, .6536, 0))
    lamp_height = .810 + (.6705 - .810) * authoring.LAMP_SCALES[level - 1]
    for i in range(4):
        a = i * math.pi / 2
        check.close(nodes[byname['socket_light_%02d' % (i + 1)]].get('translation'),
                    (-math.sin(a) * .505, lamp_height, -math.cos(a) * .505))
    visible, collision, counts, order = [], [], {}, {}
    materials = set()
    for node in nodes:
        if 'mesh' not in node:
            continue
        primitives = doc['meshes'][node['mesh']]['primitives']
        assert len(primitives) == 1
        primitive = primitives[0]
        attrs = primitive['attributes']
        positions, normals = values(attrs['POSITION']), values(attrs['NORMAL'])
        indices = [row[0] for row in values(primitive['indices'])]
        assert all(all(math.isfinite(c) for c in p) for p in positions)
        assert all(abs(Vector(n).length - 1) < 1e-4 for n in normals)
        triangles = set()
        for start in range(0, len(indices), 3):
            vertices = [Vector(positions[j]) for j in indices[start:start + 3]]
            normal = (vertices[1] - vertices[0]).cross(vertices[2] - vertices[0])
            assert normal.length > 1e-9
            assert all(normal.dot(Vector(normals[j])) > 0 for j in indices[start:start + 3]), (level, node['name'])
            key = tuple(sorted(tuple(round(c, 6) for c in v) for v in vertices))
            assert key not in triangles
            triangles.add(key)
        points = [check.node_transform(node) @ Vector(v) for v in positions]
        counts[node['name']] = len(indices) // 3
        if node['name'].startswith('COL_'):
            collision.extend(points)
            assert len(set(positions)) == 8 and len(indices) == 36
        else:
            visible.extend(points)
            order[node['name']] = len(order)
            materials.add(primitive['material'])
            for u, v in values(attrs['TEXCOORD_0']):
                assert min(abs(u - column) for column in (.84375, .59375, .65625, .96875)) < 1e-5
                assert .025 - 1e-5 <= 1 - v <= .225 + 1e-5 or .275 - 1e-5 <= 1 - v <= .475 + 1e-5
    assert order == {'base_mesh': 0, 'light_mesh': 1}
    assert len(doc['meshes']) == 3 and len(materials) == 2 and len(doc['materials']) == 3
    energy = doc['materials'][doc['meshes'][nodes[byname['light_mesh']]['mesh']]['primitives'][0]['material']]
    check.close(energy['emissiveFactor'], (.65, .65, .65))
    cb = check.bounds(collision)
    check.close(cb['min'], (-.640, 0, -.640) if level == 1 else (-.670, 0, -.670))
    check.close(cb['max'], (.640, 1.470, .640) if level == 1 else (.670, 1.530, .670))
    vb = check.bounds(visible)
    assert abs(vb['min'][1]) < 1e-6
    assert all(cb['min'][i] <= vb['min'][i] + 1e-6 and cb['max'][i] >= vb['max'][i] - 1e-6 for i in range(3))
    return {'level': level, 'description': authoring.DESCRIPTIONS[level - 1], 'file': str(path.relative_to(ROOT)),
            'sha256': hashlib.sha256(blob).hexdigest(), 'visible_bounds_glb': vb, 'collision_bounds_glb': cb,
            'visible_triangles': sum(v for k, v in counts.items() if not k.startswith('COL_')),
            'visible_meshes': 2, 'visible_primitives': 2, 'visible_materials': 2,
            'triangles_by_mesh': counts, 'render_order': order, 'transforms_glb': transforms,
            'raw_glb_verified': True, 'embedded_palette_identical': True}


def mechanical(level):
    scene = bpy.data.scenes['DECOY_BEACON_L%02d' % level]
    bpy.context.window.scene = scene
    bpy.context.view_layer.update()
    objects = {o['canonical_name']: o for o in scene.objects if o.get('canonical_name')}
    base, light = objects['base_mesh'], objects['light_mesh']
    image = next(n.image for n in base.data.materials[0].node_tree.nodes if n.type == 'TEX_IMAGE')
    assert image.packed_file and bytes(image.packed_file.data) == PALETTE

    def group_bounds(obj, name):
        return check.bounds([obj.data.vertices[i].co for i in check.ids(obj, name)])

    for i in range(4):
        top, bottom, hanger, joint, arm = [group_bounds(base, prefix + '%02d' % i) for prefix in
                                          ('decoy_lamp_top_', 'decoy_lamp_bottom_', 'decoy_hanger_', 'decoy_arm_joint_', 'decoy_arm_')]
        glass = group_bounds(light, 'decoy_lamp_glass_%02d' % i)
        assert bottom['max'][2] > glass['min'][2] and top['min'][2] < glass['max'][2]
        assert hanger['min'][2] < top['max'][2] and hanger['max'][2] > joint['min'][2]
        assert bottom['min'][2] > .329 + .120, ('lamp too close to foot', level)
        for axis in range(3):
            assert min(arm['max'][axis], joint['max'][axis]) > max(arm['min'][axis], joint['min'][axis])
        if level >= 6:
            for corner in range(4):
                guard = group_bounds(base, 'lamp_guard_%02d_%02d' % (i, corner))
                assert guard['min'][2] < bottom['max'][2] and guard['max'][2] > top['min'][2], 'guard must reach both caps'
    eye = check.tree(base, only='decoy_lifting_eye')
    assert eye.ray_cast(Vector((0, 1, 1.342)), Vector((0, -1, 0)), 2)[0] is None, 'eye hole blocked'
    # Visibility of existing outer bolt faces must survive the new gold frames.
    for i in range(4):
        a = math.pi / 4 + i * math.pi / 2
        radial = Vector((math.cos(a), math.sin(a), 0))
        armor = check.tree(base, only='decoy_armor_%02d' % i)
        points = [base.data.vertices[j].co for j in check.ids(base, 'decoy_bolt_%02d' % i)]
        front = max(p.dot(radial) for p in points)
        for point in points:
            if abs(point.dot(radial) - front) < 1e-5:
                hit, _, _, distance = armor.ray_cast(point + radial * .02, -radial, .15)
                assert hit is not None and distance > .0201, 'bolt is buried'
    return {'lamps_attached': True, 'cross_arms_attached': True, 'guard_connections_checked': level >= 6,
            'open_lifting_eye': True, 'exposed_bolts': True, 'clearance_above_feet': True, 'packed_palette': True}


def round_trip(level):
    scene = bpy.data.scenes.new('DECOY_ROUNDTRIP')
    bpy.context.window.scene = scene
    bpy.ops.import_scene.gltf(filepath=str(OUT / ('decoy_beacon_lvl%02d.glb' % level)))
    bpy.context.view_layer.update()
    points = []
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
    for i, p in enumerate(points):
        tree.insert(p, i)
    tree.balance()
    for point in points:
        assert tree.find(Vector((-point.x, point.y, point.z)))[2] < 1e-5, 'X mirror symmetry'
        assert tree.find(Vector((point.x, -point.y, point.z)))[2] < 1e-5, 'Y mirror symmetry'
    for obj in list(scene.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.scenes.remove(scene)


def main():
    assert bpy.app.background
    bpy.ops.wm.open_mainfile(filepath=str(ART / 'decoy_beacon_levels.blend'))
    rows = []
    for level in range(1, 9):
        row = raw_asset(level)
        row['mechanical'] = mechanical(level)
        round_trip(level)
        row['fresh_import_verified'] = row['normals_uv_manifold_verified'] = row['mirror_symmetry_verified'] = True
        rows.append(row)
        print('DECOY_LEVEL_VALIDATED', level, row['visible_triangles'], flush=True)
    bpy.context.window.scene = bpy.data.scenes['DECOY_BEACON_L08']
    for other in list(bpy.data.scenes):
        if not other.name.startswith('DECOY_BEACON_L'):
            bpy.data.scenes.remove(other)
    for scene in bpy.data.scenes:
        scene.use_fake_user = True
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_perspective = 'CAMERA'
            area.spaces.active.shading.type = 'MATERIAL'
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'decoy_beacon_levels.blend'))
    manifest = {'asset': 'decoy_beacon_levels', 'levels': rows,
                'mode': 'Eight complete replacement models; draw one level, not additive overlays',
                'original_base_sha256': hashlib.sha256(authoring.BASE.read_bytes()).hexdigest(),
                'source_blend_sha256': hashlib.sha256(authoring.SOURCE.read_bytes()).hexdigest(),
                'palette': {'path': 'assets/models/buildings/Textures/colormap.png',
                            'sha256': hashlib.sha256(PALETTE).hexdigest(), 'embedded_glb': True, 'packed_blend': True},
                'coordinates': 'GLB +Y up, -Z forward; Blender +Z up, +Y forward; metres',
                'footprint_m': [2, 2], 'motion': 'Static; no pivots, muzzle, recoil, rig or animation clips',
                'effect_socket_glb': [0, .6536, 0], 'light_mesh_index': 1,
                'central_radial_scales': authoring.CENTRAL_SCALES, 'hanging_lamp_scales': authoring.LAMP_SCALES,
                'license': 'Kenney Tower Defense Kit 2.1 palette CC0; original project geometry',
                'integration': {'gameplay_changed': False, 'renderer_connected': True,
                                'loader_verified': False, 'playtested': False}}
    (ART / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('DECOY_ALL_LEVELS_VERIFIED', flush=True)


if __name__ == '__main__':
    main()

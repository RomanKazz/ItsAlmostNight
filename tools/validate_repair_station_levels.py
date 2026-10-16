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
ART = ROOT / 'art/models/repair_station_levels'
OUT = ROOT / 'assets/models/buildings/repair_station_levels'
spec = importlib.util.spec_from_file_location('repair_levels_authoring', ROOT / 'tools/create_repair_station_levels.py')
authoring = importlib.util.module_from_spec(spec)
spec.loader.exec_module(authoring)
spec = importlib.util.spec_from_file_location('repair_levels_validation_helpers', ROOT / 'tools/validate_wind_blower_levels.py')
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)
PALETTE = check.PALETTE


def raw_asset(level):
    path = OUT / ('repair_station_lvl%02d.glb' % level)
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
    expected = {'asset_root', 'base_mesh', 'service_mesh', 'COL_BOX_BODY', 'socket_base',
                'socket_ui_top', 'socket_effect_repair', 'socket_service_console'}
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
    check.close(nodes[byname['socket_ui_top']].get('translation'), (0, 1.600, 0))
    check.close(nodes[byname['socket_effect_repair']].get('translation'), (0, 1.205, 0))
    check.close(nodes[byname['socket_service_console']].get('translation'), (0, .320, -.545))
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
                assert min(abs(u - column) for column in (.09375, .03125, .84375, .59375, .65625, .96875)) < 1e-5
                assert .025 - 1e-5 <= 1 - v <= .225 + 1e-5 or .275 - 1e-5 <= 1 - v <= .475 + 1e-5
    assert order == {'base_mesh': 0, 'service_mesh': 1}
    assert len(doc['meshes']) == 3 and len(materials) == 2 and len(doc['materials']) == 3
    energy = doc['materials'][doc['meshes'][nodes[byname['service_mesh']]['mesh']]['primitives'][0]['material']]
    check.close(energy['emissiveFactor'], (.35, .35, .35))
    cb = check.bounds(collision)
    check.close(cb['min'], (-.600, 0, -.710) if level == 1 else (-.660, 0, -.730))
    check.close(cb['max'], (.600, 1.500, .600) if level == 1 else (.660, 1.520, .660))
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
    scene = bpy.data.scenes['REPAIR_STATION_L%02d' % level]
    bpy.context.window.scene = scene
    bpy.context.view_layer.update()
    objects = {o['canonical_name']: o for o in scene.objects if o.get('canonical_name')}
    base, service = objects['base_mesh'], objects['service_mesh']
    image = next(n.image for n in base.data.materials[0].node_tree.nodes if n.type == 'TEX_IMAGE')
    assert image.packed_file and bytes(image.packed_file.data) == PALETTE
    wrench = check.tree(service, only='repair_wrench')
    assert wrench.ray_cast(Vector((0, 1, 1.420)), Vector((0, -1, 0)), 2)[0] is None, 'wrench jaw blocked'
    frame = check.tree(base)
    # The source display remains outside the frame and the housing.
    glass = [service.data.vertices[j].co for j in check.ids(service, 'repair_display_glass')]
    point = max(glass, key=lambda v: v.y).copy()
    hit, _, _, distance = frame.ray_cast(point + Vector((0, .020, 0)), Vector((0, -1, 0)), .30)
    assert hit is not None and distance > .0201, 'display buried by upgrade armor'
    for i in range(4):
        a = math.pi / 4 + i * math.pi / 2
        radial = Vector((math.cos(a), math.sin(a), 0))
        brace = check.tree(base, only='repair_brace_%d' % i)
        points = [base.data.vertices[j].co for j in check.ids(base, 'repair_brace_bolt_%d' % i)]
        front = max(p.dot(radial) for p in points)
        for point in points:
            if abs(point.dot(radial) - front) < 1e-5:
                hit, _, _, distance = brace.ray_cast(point + radial * .02, -radial, .15)
                assert hit is not None and distance > .0201, 'brace bolt buried'
    if level >= 5:
        for side in (-1, 1):
            bottom = [base.data.vertices[j].co.z for j in check.ids(base, 'service_lower_cap_%d' % side)]
            assert min(bottom) < .230 and max(bottom) > .230, 'reservoir not seated on the base'
    return {'open_wrench_jaw': True, 'visible_display': True, 'exposed_bolts': True,
            'reservoirs_seated': level >= 5, 'packed_palette': True}


def round_trip(level):
    scene = bpy.data.scenes.new('REPAIR_ROUNDTRIP')
    bpy.context.window.scene = scene
    bpy.ops.import_scene.gltf(filepath=str(OUT / ('repair_station_lvl%02d.glb' % level)))
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
    for obj in list(scene.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.scenes.remove(scene)


def main():
    assert bpy.app.background
    bpy.ops.wm.open_mainfile(filepath=str(ART / 'repair_station_levels.blend'))
    rows = []
    for level in range(1, 9):
        row = raw_asset(level)
        row['mechanical'] = mechanical(level)
        round_trip(level)
        row['fresh_import_verified'] = row['normals_uv_manifold_verified'] = row['mirror_symmetry_verified'] = True
        rows.append(row)
        print('REPAIR_LEVEL_VALIDATED', level, row['visible_triangles'], flush=True)
    bpy.context.window.scene = bpy.data.scenes['REPAIR_STATION_L08']
    for other in list(bpy.data.scenes):
        if not other.name.startswith('REPAIR_STATION_L'):
            bpy.data.scenes.remove(other)
    for scene in bpy.data.scenes:
        scene.use_fake_user = True
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_perspective = 'CAMERA'
            area.spaces.active.shading.type = 'MATERIAL'
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'repair_station_levels.blend'))
    manifest = {'asset': 'repair_station_levels', 'levels': rows,
                'mode': 'Eight complete replacement models; draw one level, not additive overlays',
                'original_base_sha256': hashlib.sha256(authoring.BASE.read_bytes()).hexdigest(),
                'source_blend_sha256': hashlib.sha256(authoring.SOURCE.read_bytes()).hexdigest(),
                'palette': {'path': 'assets/models/buildings/Textures/colormap.png',
                            'sha256': hashlib.sha256(PALETTE).hexdigest(), 'embedded_glb': True, 'packed_blend': True},
                'coordinates': 'GLB +Y up, -Z forward; Blender +Z up, +Y forward; metres',
                'footprint_m': [2, 2], 'motion': 'Static; no pivots, muzzle, recoil, rig or animation clips',
                'effect_socket_glb': [0, .6536, 0], 'service_mesh_index': 1,
                'wrench_width_scales': authoring.WRENCH_WIDTHS,
                'license': 'Kenney Tower Defense Kit 2.1 palette CC0; original project geometry',
                'integration': {'gameplay_changed': False, 'renderer_connected': True,
                                'loader_verified': False, 'playtested': False}}
    (ART / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('REPAIR_ALL_LEVELS_VERIFIED', flush=True)


if __name__ == '__main__':
    main()

"""Validate the preserved catapult base and the new Blender-authored overlays."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import struct

import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/catapult_levels'
spec = importlib.util.spec_from_file_location('catapult_levels_authoring', ROOT / 'tools/create_catapult_levels.py')
authoring = importlib.util.module_from_spec(spec)
spec.loader.exec_module(authoring)
spec = importlib.util.spec_from_file_location('catapult_levels_checks', ROOT / 'tools/validate_wind_blower_levels.py')
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)


def raw_checks():
    original, original_binary = authoring.read_glb(authoring.BASE)
    doc, binary = authoring.read_glb(authoring.OUT)
    assert doc['meshes'][:3] == original['meshes']
    assert doc['nodes'][:9] == original['nodes']
    assert doc['accessors'][:len(original['accessors'])] == original['accessors']
    assert doc['bufferViews'][:len(original['bufferViews'])] == original['bufferViews']
    assert binary[:original['buffers'][0]['byteLength']] == original_binary[:original['buffers'][0]['byteLength']]
    assert doc['materials'] == original['materials'] and doc['images'] == original['images']
    assert doc['textures'] == original['textures']
    assert len(doc['meshes']) == 17 and len(doc['nodes']) == 23 and len(doc['materials']) == 2
    assert not doc.get('animations') and not doc.get('skins') and not doc.get('extensionsRequired')
    assert all('uri' not in x for x in doc['buffers'] + doc['images'])
    image = doc['bufferViews'][doc['images'][0]['bufferView']]
    start = image.get('byteOffset', 0)
    assert binary[start:start + image['byteLength']] == check.PALETTE

    def values(index):
        a = doc['accessors'][index]
        v = doc['bufferViews'][a['bufferView']]
        component = {5126: 'f', 5125: 'I', 5123: 'H', 5121: 'B'}[a['componentType']]
        width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
        row = '<' + component * width
        offset = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        stride = v.get('byteStride', struct.calcsize(row))
        return [struct.unpack_from(row, binary, offset + i * stride) for i in range(a['count'])]

    rows = []
    for level in range(2, 9):
        row = {'level': level, 'description': authoring.DESCRIPTIONS[level - 1], 'parts': {}}
        for role, offset in (('base', 0), ('arm', 1)):
            index = 3 + (level - 2) * 2 + offset
            mesh = doc['meshes'][index]
            name = 'upgrade_catapult_l%d_%s' % (level, role)
            assert mesh['name'] == name and doc['nodes'][index + 6] == {'name': name, 'mesh': index}
            assert len(mesh['primitives']) == 1
            primitive = mesh['primitives'][0]
            assert primitive['material'] == 0 and 'COLOR_0' not in primitive['attributes']
            attrs = primitive['attributes']
            positions, normals = values(attrs['POSITION']), values(attrs['NORMAL'])
            indices = [v[0] for v in values(primitive['indices'])]
            assert all(all(math.isfinite(c) for c in v) for v in positions)
            assert all(abs(Vector(n).length - 1) < 1e-4 for n in normals)
            triangles = set()
            for start in range(0, len(indices), 3):
                tri = [Vector(positions[j]) for j in indices[start:start + 3]]
                normal = (tri[1] - tri[0]).cross(tri[2] - tri[0])
                assert normal.length > 1e-9
                assert all(normal.dot(Vector(normals[j])) > 0 for j in indices[start:start + 3])
                key = tuple(sorted(tuple(round(c, 6) for c in v) for v in tri))
                assert key not in triangles
                triangles.add(key)
            for u, v in values(attrs['TEXCOORD_0']):
                assert min(abs(u - c) for c in (.65625, .96875, .84375, .59375)) < 1e-5
                assert .025 - 1e-5 <= 1 - v <= .225 + 1e-5 or .275 - 1e-5 <= 1 - v <= .475 + 1e-5
            row['parts'][role] = {'node': name, 'render_index': index - 1,
                'bounds_glb': check.bounds([Vector(v) for v in positions]), 'triangles': len(indices) // 3,
                'material': 0, 'uv_palette_verified': True}
        rows.append(row)
    return rows


def source_checks(level):
    scene = bpy.data.scenes['CATAPULT_L%02d' % level]
    bpy.context.window.scene = scene
    yaw = next(o for o in scene.objects if o.get('canonical_name', '').startswith('yaw_pivot'))
    pitch = next(o for o in scene.objects if o.get('canonical_name', '').startswith('pitch_pivot'))
    yaw.rotation_mode = pitch.rotation_mode = 'XYZ'
    body = next(o for o in scene.objects if o.get('role') == 'original_carriage')
    original_arm = next(o for o in scene.objects if o.get('role') == 'original_arm')
    base = next(o for o in scene.objects if o.get('role') == 'base')
    arm = next(o for o in scene.objects if o.get('role') == 'arm')
    assert base.parent == yaw and arm.parent == pitch
    bpy.context.view_layer.update()
    image = next(n.image for n in base.data.materials[0].node_tree.nodes if n.type == 'TEX_IMAGE')
    assert image.packed_file and bytes(image.packed_file.data) == check.PALETTE
    for obj in (base, arm):
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        assert all(e.is_manifold and e.is_contiguous for e in bm.edges)
        assert bm.calc_volume(signed=True) > 0
        bm.free()
    # The original bowl and throwing animation are preserved. New rings never
    # obstruct its opening, and new carriage plates rest on the existing base.
    opening = check.tree(arm)
    assert opening.ray_cast(Vector((0, -authoring.BUCKET, 1.2)), Vector((0, 0, -1)), .6)[0] is None
    if level >= 3:
        floor = check.tree(body)
        for side in (-1, 1):
            for index in check.ids(base, 'gold_cheek_%d' % side):
                point = base.matrix_world @ base.data.vertices[index].co
                if abs(point.z - .225) < 1e-6:
                    hit, *_ = floor.ray_cast(point + Vector((0, 0, .15)), Vector((0, 0, -1)), .20)
                    assert hit is not None and hit.z > point.z, ('unsupported cheek', level, tuple(point))
    poses = set((0, -1.20 + i * .01) for i in range(139))
    poses |= set((angle, p) for angle in range(360) for p in (-1.20, 0, .18))
    intersections = []
    for angle, p in sorted(poses):
        yaw.rotation_euler.z, pitch.rotation_euler.x = math.radians(angle), p
        bpy.context.view_layer.update()
        pairs = ((arm, body), (arm, base), (original_arm, base))
        for a, b in pairs:
            if check.tree(a).overlap(check.tree(b)):
                intersections.append([angle, p, a.get('role'), b.get('role')])
    yaw.rotation_euler.z = pitch.rotation_euler.x = 0
    bpy.context.view_layer.update()
    assert not intersections, (level, intersections[:10])
    return {'poses': len(poses), 'full_yaw_step_degrees': 1, 'pitch_step_radians': .01,
            'pitch_range_radians': [-1.20, .18], 'new_part_intersections': 0,
            'packed_palette': True, 'open_bucket': True, 'supported_cheeks': level >= 3}


def round_trip():
    scene = bpy.data.scenes.new('CATAPULT_ROUNDTRIP')
    bpy.context.window.scene = scene
    bpy.ops.import_scene.gltf(filepath=str(authoring.OUT))
    bpy.context.view_layer.update()
    upgrades = [o for o in scene.objects if o.type == 'MESH' and o.name.startswith('upgrade_catapult_')]
    assert len(upgrades) == 14
    for obj in upgrades:
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-6)
        assert all(e.is_manifold and e.is_contiguous for e in bm.edges), obj.name
        assert bm.calc_volume(signed=True) > 0
        bm.free()
    muzzle = next(o for o in scene.objects if o.name.startswith('Muzzle_01'))
    check.close(muzzle.matrix_world.translation, (0, -.6370007395744324, .8762649297714233))
    for obj in list(scene.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.scenes.remove(scene)


def main():
    assert bpy.app.background
    bpy.ops.wm.open_mainfile(filepath=str(ART / 'catapult_levels.blend'))
    rows = raw_checks()
    for row in rows:
        row['source_and_motion'] = source_checks(row['level'])
        print('CATAPULT_LEVEL_VERIFIED', row['level'], flush=True)
    round_trip()
    bpy.context.window.scene = bpy.data.scenes['CATAPULT_L08']
    for scene in list(bpy.data.scenes):
        if not scene.name.startswith('CATAPULT_L'):
            bpy.data.scenes.remove(scene)
        else:
            scene.use_fake_user = True
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'catapult_levels.blend'))
    manifest = {'asset': 'catapult_levels', 'levels': [{'level': 1, 'description': authoring.DESCRIPTIONS[0]}] + rows,
        'runtime_glb': str(authoring.OUT.relative_to(ROOT)),
        'runtime_sha256': hashlib.sha256(authoring.OUT.read_bytes()).hexdigest(),
        'immutable_base_glb': str(authoring.BASE.relative_to(ROOT)),
        'base_sha256': hashlib.sha256(authoring.BASE.read_bytes()).hexdigest(),
        'original_base_binary_nodes_meshes_rig_colliders_and_muzzle_unchanged': True,
        'palette': {'path': 'assets/models/buildings/Textures/colormap.png',
                    'sha256': hashlib.sha256(check.PALETTE).hexdigest(), 'embedded_glb': True, 'packed_blend': True},
        'mode': 'Original arm/carriage plus one selected pair of overlays for levels 2 through 8',
        'render_mesh_count': 16, 'maximum_draws_per_catapult': 4,
        'coordinates': 'GLB +Y up; original catapult throws toward +Z. Blender +Z up, -Y forward',
        'pitch_pivot_glb': [0, authoring.PIVOT, 0],
        'muzzle_glb': [0, .8762649297714233, .6370007395744324],
        'footprint_cells': [2, 2], 'fresh_import_verified': True,
        'license': 'Kenney Tower Defense Kit CC0 base and original colormap; new project upgrade geometry',
        'integration': {'renderer_connected': True, 'gameplay_changed': False,
                        'loader_verified': False, 'native_graphics_smoke': False, 'playtested': False}}
    (ART / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print('CATAPULT_ALL_LEVELS_VERIFIED', flush=True)


if __name__ == '__main__':
    main()

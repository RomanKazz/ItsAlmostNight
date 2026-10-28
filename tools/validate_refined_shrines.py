"""Validate the woodland shrine runtime exports in a clean Blender process."""
from pathlib import Path
import hashlib
import json
import math
import struct

import bpy
import bmesh
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/shrines/rework'
palette = (ROOT / 'assets/models/buildings/Textures/colormap.png').read_bytes()
report = {}
for kind in ('player', 'building'):
    path = ROOT / 'assets/models/world_landmarks' / (kind + '_shrine.glb')
    data = path.read_bytes()
    length = struct.unpack_from('<I', data, 12)[0]
    doc = json.loads(data[20:20+length])
    binary = data[28+length:]
    assert len(doc['meshes']) == 2 and len(doc['materials']) == 1
    assert not doc.get('extensionsRequired')
    assert all('uri' not in buf for buf in doc['buffers'])
    assert all('uri' not in im for im in doc['images'])
    view = doc['bufferViews'][doc['images'][0]['bufferView']]
    embedded = binary[view.get('byteOffset', 0):view.get('byteOffset', 0)+view['byteLength']]
    assert embedded == palette
    assert all(len(mesh['primitives']) == 1 for mesh in doc['meshes'])
    for mesh in doc['meshes']:
        accessor = doc['accessors'][mesh['primitives'][0]['attributes']['NORMAL']]
        view = doc['bufferViews'][accessor['bufferView']]
        begin = view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
        stride = view.get('byteStride', 12)
        for i in range(accessor['count']):
            normal = struct.unpack_from('<3f', binary, begin+i*stride)
            assert all(math.isfinite(v) for v in normal)
            assert abs(sum(v*v for v in normal)-1) < 1e-4
    scene = bpy.data.scenes.new('SHRINE_REIMPORT_' + kind)
    bpy.context.window.scene = scene
    bpy.ops.import_scene.gltf(filepath=str(path))
    bpy.context.view_layer.update()
    objects = [o for o in scene.objects if o.type == 'MESH']
    assert len(objects) == 2
    trees = {}
    meshes = []
    coords = []
    for obj in objects:
        mesh = obj.data
        mesh.calc_loop_triangles()
        assert mesh.uv_layers.active is not None
        assert all(math.isfinite(v) for vertex in mesh.vertices for v in vertex.co)
        assert all(0 <= v <= 1 for uv in mesh.uv_layers.active.data for v in uv.uv)
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-6)
        assert all(e.is_manifold and e.is_contiguous for e in bm.edges), obj.name
        assert bm.calc_volume(signed=True) > 0, obj.name
        # Check each disconnected solid, so a positive total cannot hide an
        # inverted cap or a single inside-out component.
        remaining = set(bm.faces)
        solids = 0
        while remaining:
            pending = [remaining.pop()]
            component = []
            while pending:
                face = pending.pop()
                component.append(face)
                for edge in face.edges:
                    for neighbor in edge.link_faces:
                        if neighbor in remaining:
                            remaining.remove(neighbor)
                            pending.append(neighbor)
            volume = 0
            for face in component:
                points = [v.co for v in face.verts]
                for i in range(1, len(points)-1):
                    volume += points[0].dot(points[i].cross(points[i+1])) / 6
            assert volume > 1e-9, (obj.name, solids, volume)
            solids += 1
        bm.free()
        vertices = [obj.matrix_world @ v.co for v in mesh.vertices]
        coords.extend(vertices)
        trees['body' if 'body' in obj.name else 'glyph'] = BVHTree.FromPolygons(
            vertices, [tuple(t.vertices) for t in mesh.loop_triangles], all_triangles=True)
        meshes.append({'name': obj.name, 'triangles': len(mesh.loop_triangles),
                       'closed_outward_solids': solids, 'uvs_valid': True})
    crossings = trees['body'].overlap(trees['glyph'])
    assert not crossings, (kind, 'emblem clips architecture', len(crossings))
    lo = [min(v[i] for v in coords) for i in range(3)]
    hi = [max(v[i] for v in coords) for i in range(3)]
    assert abs(lo[2]) < 1e-6 and abs(hi[2] - 2.8) < 1e-5
    assert all(abs(v[i]) <= 1.0001 for v in coords for i in (0, 1))
    symmetry_errors = {}
    for label, axis in [('left_right', 0), ('front_back', 1)]:
        worst = 0
        # Check each material mesh separately; a mirrored point on a different
        # object must not hide asymmetry in the emblem or its architecture.
        for obj in objects:
            points = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
            tree = KDTree(len(points))
            for i, point in enumerate(points):
                tree.insert(point, i)
            tree.balance()
            for point in points:
                reflected = point.copy()
                reflected[axis] *= -1
                worst = max(worst, tree.find(reflected)[2])
        assert worst < 1e-5, (kind, label, worst)
        symmetry_errors[label] = worst
    report[kind] = {'meshes': meshes, 'palette_byte_identical': True,
                    'palette_sha256': hashlib.sha256(embedded).hexdigest(),
                    'unit_normals': True, 'emblem_clearance': True,
                    'floor_zero': True, 'height': hi[2], 'within_2x2_footprint': True,
                    'self_contained_glb': True, 'native_manual_playtest': False}
    report[kind]['mirrored_vertex_max_error_m'] = symmetry_errors
(ART / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))

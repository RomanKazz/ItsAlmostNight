"""Validate palette, topology and motion in a clean Blender GLB import."""
from pathlib import Path
import hashlib
import json
import math
import struct
import bpy
import bmesh
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT/'art/models/cannon_levels'
RAW = (ROOT/'assets/models/buildings/cannon.glb').read_bytes()
size = struct.unpack_from('<I',RAW,12)[0]
doc = json.loads(RAW[20:20+size])
binary = RAW[28+size:]
manifest = json.loads((ART/'manifest.json').read_text())
assert len(doc['meshes']) == 17 # collider + two original meshes + fourteen overlays
assert next(n for n in doc['nodes'] if n.get('name','').startswith('pitch_pivot'))['translation'] == manifest['pitch_pivot_glb']
for image in doc['images']:
    view = doc['bufferViews'][image['bufferView']]
    offset = view.get('byteOffset',0)
    assert hashlib.sha256(binary[offset:offset+view['byteLength']]).hexdigest() == manifest['palette_sha256']
checks = []
for mesh in doc['meshes'][3:]:
    primitive = mesh['primitives'][0]
    assert set(primitive['attributes']) == {'POSITION','NORMAL','TEXCOORD_0'}
    accessor = doc['accessors'][primitive['attributes']['NORMAL']]
    view = doc['bufferViews'][accessor['bufferView']]
    offset = view.get('byteOffset',0)+accessor.get('byteOffset',0)
    for i in range(accessor['count']):
        normal = struct.unpack_from('<fff',binary,offset+i*12)
        assert abs(sum(n*n for n in normal)-1) < 1e-5
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/models/buildings/cannon.glb'))
scene = bpy.context.scene
overlays = [o for o in scene.objects if o.name.startswith('upgrade_cannon')]
base = next(o for o in scene.objects if o.type=='MESH' and o.name.startswith('weapon-cannon'))
barrel = next(o for o in scene.objects if o.type=='MESH' and o.name.startswith('barrel'))
for obj in overlays:
    mesh = obj.data
    finite = all(math.isfinite(value) for v in mesh.vertices for value in v.co)
    uv = all(0<=value<=1 for loop in mesh.uv_layers.active.data for value in loop.uv)
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    closed = all(edge.is_manifold for edge in bm.edges)
    volume = bm.calc_volume(signed=True)
    bm.free()
    checks.append({'mesh':obj.name,'finite':finite,'uv_in_palette':uv,
                   'manifold_components':closed,'positive_volume':volume>0})
    assert finite and uv and closed and volume>0, checks[-1]

def tree(obj, transform=Matrix.Identity(4), foot_only=False):
    obj.data.calc_loop_triangles()
    vertices = [transform@obj.matrix_world@v.co for v in obj.data.vertices]
    faces = [tri.vertices[:] for tri in obj.data.loop_triangles
             if not foot_only or max(vertices[i].z for i in tri.vertices)<=.250001]
    return BVHTree.FromPolygons(vertices,faces,all_triangles=True)

# All carriage/base surfaces rotate with common yaw. Relative clearance is
# yaw invariant, so sweep each integer pitch from the actual -15 to +55 degrees.
fixed = tree(base)
foot = tree(base, foot_only=True)
pivot = Vector((0,0,.7001184225082397)) # Blender coordinates
motion = []
baseline_contacts = []
for degree in range(-15,56):
    transform = Matrix.Translation(pivot)@Matrix.Rotation(math.radians(degree),4,'X')@Matrix.Translation(-pivot)
    old = tree(barrel,transform)
    original_pairs = fixed.overlap(old)
    if original_pairs: baseline_contacts.append(degree)
    for level in range(2,9):
        yaw = next(o for o in overlays if o.name==f'upgrade_cannon_l{level}_base')
        pitched = next(o for o in overlays if o.name==f'upgrade_cannon_l{level}_barrel')
        new_pitch = tree(pitched,transform)
        yaw_tree = tree(yaw)
        contacts = {'pitch_vs_carriage':len(fixed.overlap(new_pitch)),
                    'pitch_vs_solid_foot':len(foot.overlap(new_pitch)),
                    'pitch_vs_yaw_overlay':len(yaw_tree.overlap(new_pitch)),
                    'barrel_vs_yaw_overlay':len(yaw_tree.overlap(old))}
        if any(contacts.values()): motion.append({'level':level,'pitch_deg':degree,**contacts})
assert not any(row['pitch_vs_yaw_overlay'] or row['barrel_vs_yaw_overlay'] or
               row['pitch_vs_solid_foot'] for row in motion), 'unintended motion clipping'
report = {'palette_matches':True,'clean_import':True,'meshes':checks,
          'tested_pitches_deg':[-15,55],'pitch_step_deg':1,
          'yaw_clearance':'common yaw preserves all relative clearances',
          'baseline_cradle_contact_pitch_range':[min(baseline_contacts),max(baseline_contacts)],
          'cradle_contacts_by_level':[
              {'level':level,'poses_with_contact':sum(row['level']==level for row in motion),
               'max_triangle_contacts':max(row['pitch_vs_carriage'] for row in motion if row['level']==level)}
              for level in range(2,9)],
          'new_parts_intersect_each_other':False,'barrel_intersects_new_carriage':False,
          'new_parts_intersect_solid_foot':False,
          'carriage_contacts_note':'Authored cradles contact the tube and its fitted armor; solid foot and new yaw armor are tested separately.',
          'loader_verified':False,'native_playtest':False}
(ART/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('Topology/UV/palette passed; zero new-part, foot or yaw-armor clashes across 497 poses.')

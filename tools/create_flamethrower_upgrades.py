"""Build 7 palette-textured upgrade shells; preserve the user's base GLB/blend.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_flamethrower_upgrades.py
Exports an independent component GLB, editable blend, previews and mesh metadata.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import bpy
from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/flamethrower_upgrades'
ART.mkdir(parents=True, exist_ok=True)
BASE = ROOT / 'assets/models/buildings/flamethrower.glb'
BASE_HASH = hashlib.sha256(BASE.read_bytes()).hexdigest()
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(BASE))
scene = bpy.context.scene
scene.name = 'Flamethrower upgrades'
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1
source_objects = list(scene.objects)
source_meshes = [o for o in source_objects if o.type == 'MESH' and not o.name.startswith('COL_')]
for obj in source_objects:
    if obj.name.startswith('COL_'): obj.hide_render = True

image = bpy.data.images.load(str(ROOT / 'assets/models/buildings/Textures/colormap.png'))
image.pack()
material = bpy.data.materials.new('flamethrower_upgrade_colormap')
material.use_nodes = True
shader = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value = 0
shader.inputs['Roughness'].default_value = 1
texture = material.node_tree.nodes.new('ShaderNodeTexImage')
texture.image = image
material.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
STEEL = (.84375, .025, .225)
DARK = (.59375, .025, .225)
ORANGE = (.84375, .275, .475)
RED = (.96875, .275, .475)
LIGHT = (.34375, .275, .475)

# Coordinates are declared in runtime GLB axes, then rotated into Blender.
def point(p): return (p[0], -p[2], p[1])
def finish(obj, swatch):
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.clear(); obj.data.materials.append(material)
    uv = obj.data.uv_layers.new(name='UVMap') if not obj.data.uv_layers else obj.data.uv_layers.active
    coordinates = [v.co.z for v in obj.data.vertices]
    lo, hi = min(coordinates), max(coordinates)
    for face in obj.data.polygons:
        for index in face.loop_indices:
            z = obj.data.vertices[obj.data.loops[index].vertex_index].co.z
            uv.data[index].uv = (swatch[0], swatch[1] + (swatch[2]-swatch[1]) * (z-lo)/max(1e-6,hi-lo))
    obj.select_set(False)
    return obj

def cube(name, center, size, swatch, bevel=0.012):
    bpy.ops.mesh.primitive_cube_add(size=1, location=point(center))
    obj = bpy.context.object; obj.name = name
    obj.scale = (size[0], size[2], size[1])
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        modifier = obj.modifiers.new('Rounded hard edges', 'BEVEL')
        modifier.width = bevel; modifier.segments = 1
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return finish(obj, swatch)

def ring(name, center, axis, lo, hi, inner, outer, swatch, sides=12):
    profile = [(inner,lo), (outer,lo), (outer,hi), (inner,hi)]
    vertices = []
    for r, height in profile:
        for i in range(sides):
            a = math.tau*i/sides
            p = [0,0,0]
            axes = [j for j in range(3) if j != axis]
            p[axes[0]] = r*math.cos(a); p[axes[1]] = r*math.sin(a); p[axis] = height
            vertices.append(point(tuple(p[j]+center[j] for j in range(3))))
    faces = []
    for band in range(4):
        for i in range(sides):
            j = (i+1)%sides; k = (band+1)%4
            face = (band*sides+i,k*sides+i,k*sides+j,band*sides+j)
            faces.append(face if axis == 1 else tuple(reversed(face)))
    mesh = bpy.data.meshes.new(name); mesh.from_pydata(vertices, [], faces); mesh.update()
    obj = bpy.data.objects.new(name,mesh); scene.collection.objects.link(obj)
    # Side walls smooth; horizontal caps and abrupt joins remain hard.
    for face in mesh.polygons: face.use_smooth = (face.index//sides) in (1,3)
    return finish(obj, swatch)

def cylinder(name, center, axis, lo, hi, radius, swatch, sides=12):
    # A small nonzero inner radius gives a closed, manifold capped ring and
    # avoids degenerate triangles. Its tiny bore is covered by the real cap.
    result = ring(name, center, axis, lo, hi, .015, radius, swatch, sides)
    return result

levels = {}
for level in range(2,9):
    parts = {'base': [], 'yaw': [], 'pitch': []}
    def add(role,obj): parts[role].append(obj)
    # Level 2: structural foot strap, tank strap and a nozzle reinforcement.
    add('base', ring('foot_steel', (0,0,0), 1, .145,.205,.592,.617,STEEL))
    add('yaw', ring('tank_strap', (0,0,.50), 1, .805,.842,.178,.195,STEEL))
    add('pitch', ring('nozzle_band', (0,.924,0), 2,-.543,-.500,.176,.198,STEEL))
    if level >= 3:
        add('yaw', ring('tank_upper_strap',(0,0,.50),1,1.03,1.065,.178,.197,STEEL))
        for side in (-1,1):
            add('yaw',cube('tank_side_clip',(side*.195,.935,.50),(.045,.26,.15),RED))
    if level >= 4:
        for side in (-1,1):
            add('base',cube('armored_shoulder',(side*.337,.425,.03),(.085,.35,.30),STEEL,.02))
            add('base',cube('shoulder_inlay',(side*.383,.44,.03),(.018,.21,.20),ORANGE,.008))
    if level >= 5:
        add('pitch',ring('barrel_heat_guard',(0,.924,0),2,-.425,-.325,.158,.190,STEEL))
        add('pitch',ring('orange_collar',(0,.924,0),2,-.408,-.345,.186,.212,ORANGE))
    if level >= 6:
        for side in (-1,1):
            add('yaw',cube('fuel_mount',(side*.235,.735,.5),(.22,.06,.16),DARK,.008))
            add('yaw',cylinder('side_fuel_cell',(side*.29,0,.50),1,.76,1.165,.101,RED))
            add('yaw',cylinder('fuel_cap',(side*.29,0,.50),1,1.162,1.195,.108,STEEL))
            add('yaw',ring('fuel_brace',(side*.29,0,.50),1,.88,.914,.10,.113,STEEL))
    if level >= 7:
        for side in (-1,1):
            add('yaw',cube('rear_guard',(side*.40,1.0,.64),(.054,.52,.05),STEEL,.01))
        add('yaw',cube('guard_bridge',(0,1.26,.64),(.83,.06,.05),ORANGE,.012))
        add('yaw',cube('pressure_mount',(0,1.18,.54),(.07,.20,.05),DARK,.008))
        add('yaw',cylinder('pressure_gauge',(0,1.30,0),2,.40,.49,.093,STEEL))
        add('yaw',cylinder('pressure_face',(0,1.30,0),2,.390,.405,.075,LIGHT))
        needle = cube('pressure_needle',(0,1.323,.382),(.018,.075,.016),RED,.003)
        needle.rotation_euler.y = -.45
        add('yaw',needle)
    if level >= 8:
        add('base',ring('master_foot_band',(0,0,0),1,.086,.130,.592,.625,ORANGE))
        add('pitch',ring('master_muzzle',(0,.924,0),2,-.575,-.550,.115,.210,ORANGE))
        for side in (-1,1):
            add('yaw',ring('fuel_master_band',(side*.29,0,.50),1,1.09,1.15,.10,.115,ORANGE))
            add('base',cube('master_corner',(side*.38,.59,.03),(.12,.08,.33),LIGHT,.012))
    merged = []
    for role in ('base','yaw','pitch'):
        bpy.ops.object.select_all(action='DESELECT')
        for obj in parts[role]: obj.select_set(True)
        bpy.context.view_layer.objects.active = parts[role][0]
        bpy.ops.object.join(); obj=bpy.context.object
        obj.name = f'upgrade_l{level}_{role}'
        obj.data.name = obj.name
        # Bake placement, with the root remaining neutral. Runtime owns pivots.
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        obj['upgrade_level']=level; obj['rig_group']=role
        merged.append(obj)
    levels[level] = merged

bpy.ops.object.select_all(action='DESELECT')
for objects in levels.values():
    for obj in objects: obj.select_set(True)
output = ROOT/'assets/models/buildings/flamethrower_upgrades.glb'
bpy.ops.export_scene.gltf(filepath=str(output), export_format='GLB',use_selection=True,
    export_yup=True, export_apply=True, export_materials='EXPORT', export_texcoords=True,
    export_normals=True, export_cameras=False, export_lights=False)
raw=output.read_bytes(); length=struct.unpack_from('<I',raw,12)[0]; document=json.loads(raw[20:20+length])
indices={}; bounds=[]; triangles=0
for index,mesh in enumerate(document['meshes']):
    indices[mesh['name']]=index
    accessor=document['accessors'][mesh['primitives'][0]['attributes']['POSITION']]
    bounds.append((accessor['min'],accessor['max']))
    for primitive in mesh['primitives']:
        triangles += document['accessors'][primitive['indices']]['count']//3
rows=[[indices[f'upgrade_l{level}_{role}'] for role in ('base','yaw','pitch')] for level in range(2,9)]
heights=[1.14]+[max(1.14,max(bounds[i][1][1] for i in row)) for row in rows]
header='''#pragma once
#include <array>
#include <raylib.h>
namespace ian::flamethrower_model {
// Generated by tools/create_flamethrower_upgrades.py; independent overlay GLB.
inline constexpr std::array<std::array<int, 3>, 7> UpgradeMeshes{{
'''
header += ''.join('    {{'+','.join(map(str,row))+'}},\n' for row in rows)+'}};\n'
header += 'inline constexpr std::array<BoundingBox, 21> UpgradeBounds{{\n'
def vec(values):return '{'+','.join(f'{v:.6f}F' for v in values)+'}'
header += ''.join('    {'+vec(lo)+','+vec(hi)+'},\n' for lo,hi in bounds)+'}};\n'
header += 'inline constexpr std::array<float, 8> UpgradeHeights{'+','.join(f'{v:.6f}F' for v in heights)+'};\n}\n'
(ROOT/'src/graphics/FlamethrowerUpgradeData.hpp').write_text(header)
manifest={'base_sha256':BASE_HASH,'component_glb':str(output.relative_to(ROOT)),
    'mesh_count':len(indices),'triangles_all_levels':triangles,'level_mesh_indices':rows,
    'level_heights':heights,'palette_sha256':hashlib.sha256((ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()).hexdigest(),
    'roles':['base','yaw','pitch'],'pitch_range_degrees':[-10,21],
    'base_geometry_preserved':hashlib.sha256(BASE.read_bytes()).hexdigest()==BASE_HASH}
(ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')

# Studio is separate from exported components. Render eight neutral-light views.
for objs in levels.values():
    for obj in objs:obj.hide_render=True
world=scene.world or bpy.data.worlds.new('Neutral world');scene.world=world;world.use_nodes=True
world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.10,.14,.16,1)
world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.65
scene.render.engine='CYCLES';scene.cycles.samples=24
scene.render.resolution_x=440;scene.render.resolution_y=440;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
scene.render.film_transparent=False
scene.view_settings.view_transform='Standard'
bpy.ops.object.camera_add(location=(2.6,4.6,2.7));camera=bpy.context.object;camera.name='upgrade_camera'
camera.rotation_euler=(Vector((0,0,.67))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=2.0;scene.camera=camera
for name,position,power,size in [('key',(3,4,6),550,5),('fill',(-4,2,4),330,4),('rim',(0,-4,5),400,3)]:
    bpy.ops.object.light_add(type='AREA',location=position);lamp=bpy.context.object;lamp.name='upgrade_'+name
    lamp.data.energy=power;lamp.data.shape='DISK';lamp.data.size=size
    lamp.rotation_euler=(Vector((0,0,.6))-lamp.location).to_track_quat('-Z','Y').to_euler()
for level in range(1,9):
    for current,objs in levels.items():
        for obj in objs:obj.hide_render=current!=level
    scene.render.filepath=str(ART/f'level_{level}.png')
    bpy.ops.render.render(write_still=True)
for label,position in [('front',(0,5,1.0)),('side',(5,0,1)),('back',(0,-5,1)),('top',(0,0,5))]:
    camera.location=position;camera.rotation_euler=(Vector((0,0,.67))-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(ART/f'level_8_{label}.png');bpy.ops.render.render(write_still=True)
camera.location=(2.6,4.6,2.7);camera.rotation_euler=(Vector((0,0,.67))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath=str(ART/'level_8.png')
# Bind editable shells to the imported real rig without moving them twice.
rig_names = {'base':'asset_root','yaw':'yaw_pivot','pitch':'pitch_pivot'}
for objects in levels.values():
    for obj in objects:
        pivot = bpy.data.objects.get(rig_names[obj['rig_group']])
        if pivot:
            matrix = obj.matrix_world.copy(); obj.parent = pivot; obj.matrix_world = matrix
bpy.context.view_layer.update()
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'flamethrower_upgrades.blend'))
assert manifest['base_geometry_preserved']
print(json.dumps(manifest))

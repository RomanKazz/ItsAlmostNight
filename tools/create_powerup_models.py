"""Create three chunky cartoon pickup models, a shared baked-color atlas and .blend.
Run: Blender --background --factory-startup --python tools/create_powerup_models.py
The shading is baked into palette UVs, so pickups remain readable at night.
"""
import json
import math
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'assets/models/powerups'
DEST.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
COLORS = [(41, 202, 234), (181, 247, 251), (44, 67, 87), (199, 106, 253),
          (246, 205, 255), (255, 190, 30), (255, 236, 141), (177, 92, 26)]
atlas = bpy.data.images.new('powerup_palette', width=128, height=128, alpha=False)
pixels = []
for y in range(128):
    shade = .52 + .65 * y / 127
    for x in range(128):
        color = COLORS[x // 16]
        pixels.extend([min(1, c / 255 * shade) for c in color] + [1])
atlas.pixels = pixels
atlas.filepath_raw = str(DEST / 'palette.png')
atlas.file_format = 'PNG'
atlas.save()
atlas.pack()
material = bpy.data.materials.new('CartoonPowerupPalette')
material.use_nodes = True
nodes = material.node_tree.nodes
shader = nodes.get('Principled BSDF')
shader.inputs['Roughness'].default_value = .8
texture = nodes.new('ShaderNodeTexImage')
texture.image = atlas
texture.interpolation = 'Linear'
material.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])

collections = []
current = None

def finish(obj, color, bevel=0, smooth=False):
    for col in list(obj.users_collection): col.objects.unlink(obj)
    current.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        modifier = obj.modifiers.new('Soft edges', 'BEVEL')
        modifier.width = bevel
        modifier.segments = 2
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for poly in obj.data.polygons: poly.use_smooth = smooth
    if bevel:
        modifier = obj.modifiers.new('Weighted corner normals', 'WEIGHTED_NORMAL')
        modifier.keep_sharp = True
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.data.materials.clear()
    obj.data.materials.append(material)
    obj['palette_color'] = color
    return obj

def cube(name, location, scale, color, bevel=.025):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    return finish(obj, color, bevel, True)

def prism(name, points, depth, color, y=0, bevel=.015):
    clean=[]
    for point in points:
        if not clean or math.dist(point,clean[-1])>1e-6: clean.append(point)
    points=clean
    if sum(points[i][0]*points[(i+1)%len(points)][1]-points[(i+1)%len(points)][0]*points[i][1]
           for i in range(len(points)))<0: points=list(reversed(points))
    count = len(points)
    vertices = [(x, y-depth/2, z) for x,z in points] + [(x, y+depth/2, z) for x,z in points]
    faces = [tuple(range(count)), tuple(reversed(range(count, count*2)))]
    faces += [(i,i+count,(i+1)%count+count,(i+1)%count) for i in range(count)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    current.objects.link(obj)
    return finish(obj, color, bevel, True)

def rings(name, profile, color, ratio=1, segments=20):
    vertices = [(math.cos(i*math.tau/segments)*r, math.sin(i*math.tau/segments)*r*ratio, z)
                for z,r in profile for i in range(segments)]
    faces = [tuple(reversed(range(segments))), tuple(range((len(profile)-1)*segments,len(profile)*segments))]
    for j in range(len(profile)-1):
        for i in range(segments):
            n=(i+1)%segments
            faces.append((j*segments+i,j*segments+n,(j+1)*segments+n,(j+1)*segments+i))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj=bpy.data.objects.new(name,mesh)
    current.objects.link(obj)
    return finish(obj,color,0,True)

def coin(name, center, radius, tilt):
    bpy.ops.mesh.primitive_cylinder_add(vertices=20, radius=radius, depth=.065, location=center,
                                      rotation=(math.pi/2,0,tilt))
    obj=bpy.context.object;obj.name=name
    finish(obj,5,.012,True)
    # Raised, cream coin center and a golden five-point star, on both faces.
    for side in [-1,1]:
        y=center[1]+side*.039
        bpy.ops.mesh.primitive_cylinder_add(vertices=20,radius=radius*.76,depth=.01,
                                          location=(center[0],y,center[2]),rotation=(math.pi/2,0,0))
        obj=bpy.context.object;obj.name=name+' inset'
        finish(obj,6,.004,True)
        star=[]
        for i in range(10):
            a=math.pi/2+i*math.pi/5;r=radius*(.53 if i%2==0 else .24)
            star.append((center[0]+math.cos(a)*r,center[2]+math.sin(a)*r))
        prism(name+' star',star,.01,5,y+side*.009,.003)

for name in ['magnet','fury','gold-rush']:
    current=bpy.data.collections.new('POWERUP_'+name)
    bpy.context.scene.collection.children.link(current)
    collections.append(current)
    if name=='magnet':
        # Open horseshoe: two chunky poles, curved heel, silver-white caps.
        points=[(-.38,.78),(-.38,.32)]
        for i in range(13):
            a=math.pi+i*math.pi/12
            points.append((math.cos(a)*.38,.32+math.sin(a)*.32))
        points += [(.38,.78),(.16,.78),(.16,.32)]
        for i in range(13):
            a=math.tau-i*math.pi/12
            points.append((math.cos(a)*.16,.32+math.sin(a)*.13))
        points += [(-.16,.78)]
        prism('Cyan horseshoe',points,.24,0,bevel=.025)
        for side in [-1,1]:
            cube('Silver pole cap',(side*.27,0,.72),(.25,.285,.19),1)
            cube('Dark pole seam',(side*.27,0,.61),(.235,.26,.032),2,.008)
            cube('Pole highlight',(side*.27,-.15,.76),(.155,.018,.036),6,.008)
        for side in [-1,1]:
            prism('Heel badge',[(-.075,.18),(0,.25),(.075,.18),(0,.1)],.025,6,side*.14,.008)
    elif name=='fury':
        bolt=[(.12,.86),(-.32,.39),(-.07,.39),(-.19,0),(.34,.53),(.08,.53),(.23,.86)]
        center=Vector((.0,.43))
        border=[tuple(center+(Vector(p)-center)*1.12) for p in bolt]
        prism('Indigo lightning rim',border,.21,2,bevel=.018)
        prism('Violet lightning core',bolt,.255,3,bevel=.022)
        gleam=[(.075,.78),(-.15,.45),(.0,.45),(-.025,.30),(.17,.57),(.035,.57)]
        for side in [-1,1]:
            prism('Pale lightning face',gleam,.012,4,side*.142,.009)
    else:
        rings('Gold pouch',[(.03,.16),(.07,.25),(.18,.32),(.36,.30),(.48,.21),(.54,.13)],5,.82)
        rings('Pouch mouth',[(.49,.14),(.55,.18),(.62,.20)],7,.82)
        rings('Cream folded lip',[(.57,.18),(.62,.21),(.65,.19)],6,.82)
        rings('Cord cinch',[(.48,.135),(.515,.155),(.535,.145)],7,.9)
        for side in [-1,1]:
            # Large raised star emblem keeps the pouch recognisable from either side.
            points=[]
            for i in range(10):
                a=math.pi/2+i*math.pi/5;r=.135 if i%2==0 else .061
                points.append((math.cos(a)*r,.28+math.sin(a)*r))
            prism('Pouch star emblem',points,.026,6,side*.272,.005)
        cube('Bow knot',(.145,-.075,.51),(.09,.07,.08),7,.018)
        for side in [-1,1]:
            cube('Bow loop',(.145+side*.06,-.065,.515),(.09,.04,.045),6,.012)
        coin('Left coin',(-.105,.025,.69),.14,-.25)
        coin('Right coin',(.09,.075,.74),.145,.15)

# Palette UV shading from smoothed corner normals. Runtime can render without
# world lighting: silhouettes stay bright at night, with soft bevels and depth.
light=Vector((-.35,-.5,.79)).normalized()
report={}
for col in collections:
    bpy.ops.object.select_all(action='DESELECT')
    for obj in col.objects:
        obj.select_set(True)
        mesh=obj.data
        uv=mesh.uv_layers.active or mesh.uv_layers.new(name='UVMap')
        color=obj['palette_color']
        normals=mesh.corner_normals
        for polygon in mesh.polygons:
            for i in polygon.loop_indices:
                normal=(obj.matrix_world.to_3x3() @ normals[i].vector).normalized()
                shade=.32+.58*max(0,normal.dot(light))
                uv.data[i].uv=((color+.5)/8,shade)
    name=col.name.removeprefix('POWERUP_')
    # Export one merged mesh/material per pickup; keep editable parts in .blend.
    bpy.ops.object.duplicate(linked=False)
    bpy.context.view_layer.objects.active=bpy.context.selected_objects[0]
    bpy.ops.object.join()
    runtime=bpy.context.object
    runtime.name='pickup_'+name
    bpy.ops.export_scene.gltf(filepath=str(DEST/(name+'.glb')),export_format='GLB',
                             use_selection=True,export_normals=True,export_yup=True,
                             export_materials='EXPORT',export_texcoords=True)
    bpy.ops.object.delete()
    triangles=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in col.objects)
    report[name]={'triangles':triangles,'parts':len(col.objects)}
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'art/models/powerups.blend'))
(DEST/'model-report.json').write_text(json.dumps(report,indent=2)+'\n')
print('POWERUP_MODELS',json.dumps(report))

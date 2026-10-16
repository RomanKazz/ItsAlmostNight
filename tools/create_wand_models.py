"""Build the two held wands using the original KayKit weapons atlas.

Run in a fresh background Blender. Keeps the original grip, replaces only
the head, exports one primitive per wand and packs the editable source.
"""
from pathlib import Path
import json
import math
import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/wands'
OUT = ROOT / 'assets/models/weapons/elemental_wands'
ART.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(ROOT / 'assets/models/weapons/ice_wand/wand.glb'))
original = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
material = original.data.materials[0]
image = next(n.image for n in material.node_tree.nodes if n.type == 'TEX_IMAGE')
image.pack()

# Blender +Z is glTF/game +Y. The hand grip and origin stay unchanged.
bm = bmesh.new()
bm.from_mesh(original.data)
bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z > .166], context='VERTS')
bm.to_mesh(original.data)
bm.free()
original.name = 'Original_KayKit_grip'
original.data.name = 'Preserved_grip'
# Each atlas cell is 1/8 wide and 1/4 high: never cross a cell boundary.
palette = {'steel': (.6875, .52, .72), 'gold': (.3125, .55, .72),
           'ice': (.0625, .03, .23), 'frost': (.8125, .05, .23),
           'ember': (.1875, .52, .72), 'hot': (.5625, .13, .23)}


def mesh(name, vertices, faces, color):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.materials.append(material)
    data.update()
    uv = data.uv_layers.new(name=original.data.uv_layers.active.name)
    u, low, high = palette[color]
    zmin = min(v[2] for v in vertices)
    zmax = max(v[2] for v in vertices)
    for loop in data.loops:
        z = vertices[loop.vertex_index][2]
        uv.data[loop.index].uv = (u, low + (high-low)*(z-zmin)/max(1e-6,zmax-zmin))
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    bm = bmesh.new(); bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data); bm.free()
    return obj

def lathe(name, rings, color, sides=8, offset=0):
    vertices = [(r*math.cos(offset+i*2*math.pi/sides)+x,
                 r*math.sin(offset+i*2*math.pi/sides), z)
                for z,r,x in rings for i in range(sides)]
    faces = [tuple(reversed(range(sides))), tuple(range((len(rings)-1)*sides,len(vertices)))]
    for j in range(len(rings)-1):
        for i in range(sides):
            a=j*sides+i; b=j*sides+(i+1)%sides
            faces.append((a,b,b+sides,a+sides))
    return mesh(name, vertices, faces, color)

def claw(name, angle, path, color):
    # Four-sided solid struts with a deliberately tapered end.
    radial = Vector((math.cos(angle),math.sin(angle),0))
    tangent = Vector((-math.sin(angle),math.cos(angle),0))
    vertices=[]
    for z,r,width in path:
        center=radial*r+Vector((0,0,z))
        for dr,dt in ((-1,-1),(1,-1),(1,1),(-1,1)):
            vertices.append(tuple(center+radial*(dr*width*.45)+tangent*(dt*width*.5)))
    faces=[(3,2,1,0),tuple(range(len(vertices)-4,len(vertices)))]
    for j in range(len(path)-1):
        for i in range(4):
            a=j*4+i; b=j*4+(i+1)%4
            faces.append((a,b,b+4,a+4))
    return mesh(name,vertices,faces,color)

models={}
for element in ('ice','fire'):
    collection=bpy.data.collections.new(element.upper()+'_WAND')
    bpy.context.scene.collection.children.link(collection)
    grip=original.copy();grip.data=original.data.copy();grip.name=element+'_grip'
    collection.objects.link(grip)
    before=set(bpy.context.scene.objects)
    lathe(element+'_neck',[(.142,.033,0),(.177,.029,0),(.205,.026,0),(.235,.045,0)],'steel' if element=='ice' else 'gold')
    lathe(element+'_collar',[(.225,.045,0),(.234,.053,0),(.25,.053,0),(.26,.044,0)],'steel' if element=='ice' else 'gold')
    if element=='ice':
        lathe('Ice_crown_crystal',[(.253,.022,0),(.286,.055,0),(.357,.065,0),(.391,.044,0),(.457,.002,0)],'ice',6,math.pi/6)
        for i in range(3):
            claw('Frost_prong_'+str(i),i*2*math.pi/3,
                 [(.24,.044,.022),(.285,.075,.019),(.327,.073,.014),(.351,.062,.004)],'steel')
        lathe('Frost_crystal_cap',[(.252,.018,0),(.269,.025,0),(.286,.012,0)],'frost',6)
    else:
        lathe('Ember_flame',[(.258,.023,0),(.29,.049,0),(.345,.054,0),(.388,.036,.008),(.447,.002,.019)],'ember',8)
        for i in range(4):
            claw('Golden_flame_prong_'+str(i),math.pi/4+i*math.pi/2,
                 [(.24,.045,.025),(.282,.073,.023),(.335,.077,.017),(.385,.062,.004)],'gold')
    for obj in set(bpy.context.scene.objects)-before:
        for old in list(obj.users_collection): old.objects.unlink(obj)
        collection.objects.link(obj)
    models[element]=list(collection.objects)
    # Export joined temporary copy; source keeps the named editable parts.
    bpy.ops.object.select_all(action='DESELECT')
    copies=[]
    for part in models[element]:
        copy=part.copy();copy.data=part.data.copy();bpy.context.collection.objects.link(copy)
        copy.select_set(True);copies.append(copy)
    bpy.context.view_layer.objects.active=copies[0]
    bpy.ops.object.join()
    joined=bpy.context.object;joined.name=element+'_wand'
    bpy.ops.export_scene.gltf(filepath=str(OUT/(element+'_wand.glb')),
        export_format='GLB',use_selection=True,export_animations=False,export_yup=True)
    bpy.data.objects.remove(joined,do_unlink=True)
bpy.data.objects.remove(original,do_unlink=True)

# Reference panel: both new wands plus the previous wand and club, same light.
for element,x in (('ice',-.36),('fire',-.12)):
    for obj in models[element]:obj.location.x=x
for path,x in (('assets/models/weapons/ice_wand/wand.glb',.12),('assets/models/tools/club.glb',.36)):
    bpy.ops.import_scene.gltf(filepath=str(ROOT/path))
    for obj in bpy.context.selected_objects:
        obj.location.x+=x;obj.name='REFERENCE_'+obj.name
scene=bpy.context.scene;scene.name='IAN_ELEMENTAL_WANDS'
scene.render.engine='BLENDER_EEVEE'
scene.render.resolution_x=1440;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Neutral studio');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.065,.085,.11,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.8
for loc,power,size in [((1,-3,4),400,4),((-2,-1,2),180,3)]:
    bpy.ops.object.light_add(type='AREA',location=loc)
    light=bpy.context.object;light.name='STUDIO_light';light.data.energy=power;light.data.size=size
    light.rotation_euler=(Vector((0,0,.2))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(.6,-2.4,1.1))
camera=bpy.context.object;camera.name='STUDIO_camera';camera.data.type='ORTHO';camera.data.ortho_scale=1.13;scene.camera=camera
for label,loc in [('comparison',(.45,-2.4,1.05)),('front',(0,-3,.23)),('side',(3,0,.23)),('back',(0,3,.23)),('top',(0,0,3))]:
    for obj in scene.objects:
        if obj.name.startswith('REFERENCE_'): obj.hide_render=label!='comparison'
    if label!='comparison':
        for element,shift in (('ice',-.14),('fire',.14)):
            for obj in models[element]:
                obj.location=(0,shift,0) if label=='side' else (shift,0,0)
    camera.location=loc;camera.rotation_euler=(Vector((0,0,.23))-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(ART/(label+'.png'));bpy.ops.render.render(write_still=True)
# Editable models return to their runtime origins. Studio references hidden.
for parts in models.values():
    for obj in parts:obj.location.x=0
for obj in scene.objects:
    if obj.name.startswith('REFERENCE_'):obj.hide_render=True;obj.hide_set(True)
for obj in models['fire']:obj.hide_render=True;obj.hide_set(True)
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'elemental_wands.blend'))
(ART/'manifest.json').write_text(json.dumps({
    'generator':'tools/create_wand_models.py','units':'metres','game_up':'+Y',
    'origin':'unchanged KayKit hand-grip origin','socket_game':[0,.35,0],
    'palette':'assets/models/weapons/ice_wand/weapons_bits_texture.png',
    'models':{e:str(OUT.relative_to(ROOT)/(e+'_wand.glb')) for e in models},
    'reference_order':['new ice','new fire','old wand','club'],
    'source_collections':['ICE_WAND','FIRE_WAND']},indent=2)+'\n')

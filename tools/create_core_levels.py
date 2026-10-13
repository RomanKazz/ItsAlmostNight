"""Author core levels in an isolated Blender scene (also runnable through Blender MCP).

Never modifies the source core.glb or another open scene. Units match original
core authoring; the existing runtime scale remains 1.76. Export has 13 meshes:
original five crystals, original level-one pedestal, seven authored pedestals.
"""
from pathlib import Path
import hashlib
import json
import math
import struct

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/core_levels'
ART.mkdir(parents=True, exist_ok=True)
BASE = ROOT / 'assets/models/buildings/core.glb'
PALETTE = ROOT / 'assets/models/buildings/Textures/colormap.png'
OUTPUT = ROOT / 'assets/models/buildings/core_levels.glb'
previous=bpy.data.scenes.get('IAN_CORE_LEVELS')
if previous:
    # Only this generator's scene; the user's live LASER scene is untouched.
    for obj in list(previous.objects):bpy.data.objects.remove(obj,do_unlink=True)
    bpy.data.scenes.remove(previous)
scene = bpy.data.scenes.new('IAN_CORE_LEVELS')
bpy.context.window.scene = scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1
model_collection = bpy.data.collections.new('CORE_RUNTIME_VARIANTS')
scene.collection.children.link(model_collection)
preview_collection = bpy.data.collections.new('CORE_LEVEL_PREVIEWS')
scene.collection.children.link(preview_collection)
root = bpy.data.objects.new('core_asset_root', None)
model_collection.objects.link(root)

bpy.ops.import_scene.gltf(filepath=str(BASE))
source = [o for o in scene.objects if o.type == 'MESH']
originals = {}
for obj in source:
    name = obj.data.name.split('.')[0]
    # Imports into a live file can acquire suffixes; identify geometry by
    # its original node name before baking, not by globally unique object name.
    if obj.name.startswith('core_upgrade_band'):
        continue
    zmax = max((obj.matrix_world @ v.co).z for v in obj.data.vertices)
    if obj.name.startswith('tower-round-crystals'):
        key = 'core_pedestal_l1'
    elif obj.name.startswith('crystal-small'):
        x, y = obj.matrix_world.translation.xy
        index = 1 if y > .1 else 2 if x > .1 else 3 if y < -.1 else 4
        key = f'core_satellite_{index}'
    else:
        key = 'core_crystal'
    copy = bpy.data.objects.new(key, obj.data.copy())
    model_collection.objects.link(copy)
    copy.data.transform(obj.matrix_world)
    copy.parent = root
    originals[key] = copy
for obj in list(scene.objects):
    if obj != root and obj not in originals.values():
        bpy.data.objects.remove(obj, do_unlink=True)

image = bpy.data.images.load(str(PALETTE), check_existing=False)
image.pack()
material = bpy.data.materials.new('core_levels_colormap')
material.use_nodes = True
bsdf = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
bsdf.inputs['Metallic'].default_value = 0
bsdf.inputs['Roughness'].default_value = 1
texture = material.node_tree.nodes.new('ShaderNodeTexImage')
texture.image = image
material.node_tree.links.new(texture.outputs['Color'], bsdf.inputs['Base Color'])
for obj in originals.values():
    obj.data.materials.clear()
    obj.data.materials.append(material)

STEEL = (.84375, .035, .215)
DARK = (.59375, .035, .215)
ORANGE = (.84375, .285, .465)
RED = (.96875, .285, .465)
PURPLE = (.46875, .535, .715)


class Geometry:
    def __init__(self):
        self.vertices, self.faces, self.swatches, self.smooth, self.groups = [], [], [], [], {}

    def face(self, indices, swatch, smooth=False):
        self.faces.append(indices)
        self.swatches.append(swatch)
        self.smooth.append(smooth)

    def lathe(self, name, profile, swatches, sides=16, matrix=Matrix.Identity(4)):
        """Closed radial profile: smooth slopes/walls, flat horizontal ledges.

        Rings share topology; flat caps split normals at export automatically.
        UV gradients span each material band independently.
        """
        start = len(self.vertices)
        for radius, z in profile:
            for i in range(sides):
                angle = math.tau * i / sides
                self.vertices.append(matrix @ Vector((radius*math.cos(angle), radius*math.sin(angle), z)))
        for band in range(len(profile)):
            following = (band+1) % len(profile)
            smooth = abs(profile[band][1]-profile[following][1]) > 1e-6
            for i in range(sides):
                j = (i+1) % sides
                self.face((start+band*sides+i, start+band*sides+j,
                           start+following*sides+j, start+following*sides+i),
                          swatches[band] if isinstance(swatches, list) else swatches, smooth)
        self.groups[name] = list(range(start, len(self.vertices)))

    def brace(self, name, angle, bottom, top, width, swatch):
        # Radial steel petals rise from the shoe into the collar. Six-sided
        # profile, bevelled shoulder; no coplanar orange/metal overlays.
        profile = [(.40, bottom), (.51, bottom), (.48, bottom+.045),
                   (.35, top), (.31, top), (.35, bottom+.055)]
        start = len(self.vertices)
        rot = Matrix.Rotation(angle, 4, 'Z')
        for tangent in (-width*.5, width*.5):
            self.vertices.extend(rot @ Vector((radius, tangent, z)) for radius,z in profile)
        count = len(profile)
        self.face(tuple(start+i for i in range(count)), swatch)
        self.face(tuple(start+count+i for i in reversed(range(count))), swatch)
        for i in range(count):
            j=(i+1)%count
            self.face((start+i,start+count+i,start+count+j,start+j),swatch)
        self.groups[name] = list(range(start,len(self.vertices)))

    def object(self, name):
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.vertices, [], self.faces)
        mesh.update()
        mesh.materials.append(material)
        uv=mesh.uv_layers.new(name='UVMap')
        for polygon,swatch,smooth in zip(mesh.polygons,self.swatches,self.smooth):
            polygon.use_smooth=smooth
            heights=[mesh.vertices[mesh.loops[i].vertex_index].co.z for i in polygon.loop_indices]
            lo,hi=min(heights),max(heights)
            for i,z in zip(polygon.loop_indices,heights):
                uv.data[i].uv=(swatch[0],swatch[1]+(swatch[2]-swatch[1])*(z-lo)/max(hi-lo,1e-6))
        # Keep smooth walls separate from flat ledges/caps when adjacent wall
        # profiles bend sharply. Blender exports the loop split normals.
        for edge in mesh.edges:
            a,b=(mesh.vertices[v].co for v in edge.vertices)
            if abs(a.z-b.z)<1e-6: edge.use_edge_sharp=True
        obj=bpy.data.objects.new(name,mesh)
        model_collection.objects.link(obj)
        obj.parent=root
        for group,indices in self.groups.items():
            obj.vertex_groups.new(name=group).add(indices,1,'REPLACE')
        return obj


pedestals=[originals['core_pedestal_l1']]
for level in range(2,9):
    g=Geometry()
    height=.207762+.035*(level-1)
    # One substantial tapered body, a wide steel shoe and a real basin. Not
    # repeated copies of the original pedestal. The inner bowl is open above.
    profile=[(.015,0),(.48,0),(.50,.025),(.50,.055),(.45,.075),
             (.40,.09),(.34,height-.065),(.36,height-.065),
             (.365,height-.045),(.365,height-.015),(.35,height),
             (.285,height),(.275,height-.055),(.015,height-.075)]
    colors=[DARK,STEEL,STEEL,STEEL,DARK,RED,ORANGE,ORANGE,STEEL,STEEL,STEEL,DARK,DARK,DARK]
    g.lathe('reactor_shoe_body_and_basin',profile,colors)
    for i in range(4):
        angle=math.pi*.25+i*math.pi*.5
        g.brace(f'steel_support_{i+1}',angle,.035,height-.035,.055 if level<4 else .085,STEEL)
    if level>=3:
        for i in range(4):
            angle=i*math.pi*.5
            # Octagonal energy window, facing out; dark surround + purple
            # inset physically separated by .006 m, no painted fake shadow.
            m=Matrix.Translation((.377*math.cos(angle),.377*math.sin(angle),.105+(height-.24)*.45)) @ Matrix.Rotation(angle,4,'Z') @ Matrix.Rotation(math.pi*.5,4,'Y')
            g.lathe(f'window_frame_{i+1}',[(.01,-.018),(.059,-.018),(.066,-.01),(.066,.01),(.052,.018),(.01,.018)],DARK,8,m)
            g.lathe(f'window_crystal_{i+1}',[(.01,.019),(.046,.019),(.045,.026),(.025,.04),(.01,.041)],PURPLE,8,m)
    if level>=4:
        for i in range(4):
            angle=math.pi*.25+i*math.pi*.5
            m=Matrix.Translation((.46*math.cos(angle),.46*math.sin(angle),.085)) @ Matrix.Rotation(angle,4,'Z') @ Matrix.Rotation(math.pi*.5,4,'Y')
            g.lathe(f'anchor_bolt_{i+1}',[(.007,-.009),(.026,-.009),(.029,.006),(.022,.018),(.007,.018)],DARK,8,m)
    if level>=5:
        # Two restrained steel waist bands support the taller reactor.
        g.lathe('waist_band',[(.35,.14),(.407,.14),(.407,.169),(.35,.169)],STEEL)
    if level>=6:
        for i in range(4):
            angle=i*math.pi*.5
            m=Matrix.Translation((.30*math.cos(angle),.30*math.sin(angle),height))
            g.lathe(f'collar_terminal_{i+1}',[(.012,-.03),(.041,-.03),(.044,.008),(.028,.035),(.012,.035)],ORANGE,8,m)
    if level>=7:
        for i in range(4):
            angle=math.pi*.25+i*math.pi*.5
            m=Matrix.Translation((.43*math.cos(angle),.43*math.sin(angle),.105))
            g.lathe(f'energy_anchor_{i+1}',[(.012,-.05),(.037,-.05),(.037,.015),(.014,.065),(.012,.065)],PURPLE,6,m)
    if level==8:
        g.lathe('master_collar',[(.278,height-.012),(.36,height-.012),(.379,height+.012),(.368,height+.039),(.30,height+.039),(.278,height+.016)],STEEL)
        g.lathe('orange_collar_crown',[(.365,height+.01),(.386,height+.01),(.386,height+.025),(.365,height+.025)],ORANGE)
    obj=g.object(f'core_pedestal_l{level}')
    obj['core_level']=level
    pedestals.append(obj)

ordered=[originals['core_crystal']]+[originals[f'core_satellite_{i}'] for i in range(1,5)]+pedestals
for o in ordered+[root]:o.select_set(True)
bpy.context.view_layer.objects.active=ordered[0]
bpy.ops.export_scene.gltf(filepath=str(OUTPUT),export_format='GLB',use_selection=True,use_active_scene=True,
                          export_yup=True,export_materials='EXPORT',export_extras=True)

# Stable runtime indices are explicit, independent of Blender name suffixes or
# traversal order. Remap mesh references only; accessors and original PNG remain.
data=OUTPUT.read_bytes();length=struct.unpack_from('<I',data,12)[0]
doc=json.loads(data[20:20+length]);binary=data[28+length:]
old_meshes=doc['meshes'];names=[o.name for o in ordered]
by_name={node['name']:node['mesh'] for node in doc['nodes'] if 'mesh' in node}
indices=[by_name[n] for n in names]
remap={old:new for new,old in enumerate(indices)}
doc['meshes']=[old_meshes[i] for i in indices]
for node in doc['nodes']:
    if 'mesh' in node:node['mesh']=remap[node['mesh']]
# raylib flattens primitives in NODE traversal order, not mesh-array order.
# Normalize both and remap every graph reference, including scene roots.
nodes=doc['nodes']
node_order=sorted(range(len(nodes)),key=lambda i:nodes[i].get('mesh',1000+i))
node_remap={old:new for new,old in enumerate(node_order)}
doc['nodes']=[nodes[i] for i in node_order]
for node in doc['nodes']:
    if 'children' in node:node['children']=[node_remap[i] for i in node['children']]
for exported_scene in doc['scenes']:
    exported_scene['nodes']=[node_remap[i] for i in exported_scene.get('nodes',[])]
json_bytes=json.dumps(doc,separators=(',',':')).encode();json_bytes+=b' '*((-len(json_bytes))%4)
OUTPUT.write_bytes(struct.pack('<III',0x46546c67,2,28+len(json_bytes)+len(binary))+
                  struct.pack('<II',len(json_bytes),0x4e4f534a)+json_bytes+
                  struct.pack('<II',len(binary),0x004e4942)+binary)

for obj in ordered:
    obj.hide_render=True
    obj.hide_set(True)

def clone(obj,name,pose):
    copy=bpy.data.objects.new(name,obj.data)
    preview_collection.objects.link(copy)
    copy.matrix_world=pose
    return copy

for level in range(1,9):
    x=((level-1)%4)*1.35-2.025;y=((level-1)//4)*1.75-.875
    offset=Matrix.Translation((x,y,0))
    p=level-1;lift=p*.035;growth=1+p*.045
    clone(pedestals[level-1],f'Preview_L{level}_pedestal',offset)
    pose=Matrix.Translation((0,0,.267762+lift)) @ Matrix.Diagonal((1+(growth-1)*.5,1+(growth-1)*.5,growth,1)) @ Matrix.Translation((0,0,-.267762))
    clone(originals['core_crystal'],f'Preview_L{level}_core',offset@pose)
    count=4 if level<3 else 6 if level<6 else 8
    for i in range(count):
        a=math.tau*i/count
        pose=Matrix.Translation((math.sin(a)*(.29+p*.008),math.cos(a)*(.29+p*.008),.491512+lift)) @ Matrix.Rotation(a,4,'Z') @ Matrix.Diagonal((1+p*.025,1+p*.025,1+p*.025,1)) @ Matrix.Translation((0,-.25,-.441512))
        clone(originals['core_satellite_1'],f'Preview_L{level}_orb_{i+1}',offset@pose)
    font=bpy.data.curves.new(f'L{level}_label','FONT');font.body=f'LEVEL {level}';font.align_x='CENTER';font.size=.14
    label=bpy.data.objects.new(f'L{level}_label',font);scene.collection.objects.link(label);label.location=(x,y-.62,.005)

world=bpy.data.worlds.new('Core_Studio');world.use_nodes=True
world.node_tree.nodes.get('Background').inputs[0].default_value=(.045,.055,.075,1)
world.node_tree.nodes.get('Background').inputs[1].default_value=.75;scene.world=world
for name,position,energy,size in [('core_key',(2,-4,6),500,5),('core_fill',(-4,-1,3),350,4),('core_rim',(0,4,4),450,4)]:
    light=bpy.data.lights.new(name,'AREA');light.energy=energy;light.shape='DISK';light.size=size
    obj=bpy.data.objects.new(name,light);scene.collection.objects.link(obj);obj.location=position
    obj.rotation_euler=(-obj.location).to_track_quat('-Z','Y').to_euler()
camera=bpy.data.objects.new('Core_Camera',bpy.data.cameras.new('Core_Camera'))
scene.collection.objects.link(camera);scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=6.0
camera.location=(3,-7,5);camera.rotation_euler=(Vector((0,0,.4))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='Standard'
scene.render.filepath=str(ART/'levels.png')
try:scene.render.engine='BLENDER_EEVEE'
except TypeError:pass
bpy.data.libraries.write(str(ART/'core_levels.blend'),{scene},fake_user=True)
manifest={'source':'core.glb (original preserved)','source_sha256':hashlib.sha256(BASE.read_bytes()).hexdigest(),
          'glb':str(OUTPUT.relative_to(ROOT)),'palette_sha256':hashlib.sha256(PALETTE.read_bytes()).hexdigest(),
          'runtime_scale':1.76,'authoring':'Blender MCP; isolated IAN_CORE_LEVELS scene',
          'axes':'Blender +Z up, GLB +Y up; symmetric core, no directional front',
          'mesh_order':names,'levels':{},'license':'Kenney source geometry/palette CC0; new authored geometry project-owned'}
for level,obj in enumerate(pedestals,1):
    coords=[v.co for v in obj.data.vertices]
    manifest['levels'][str(level)]={'pedestal_mesh':level+4,'pedestal_min':[min(v[i] for v in coords) for i in (0,2,1)],
        'pedestal_max':[max(v[i] for v in coords) for i in (0,2,1)],'crystal_lift':(level-1)*.035,'crystal_growth':1+(level-1)*.045,
        'satellites':4 if level<3 else 6 if level<6 else 8}
(ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Authored and exported 8 core levels, 13 meshes. Original source and other Blender scenes preserved.')

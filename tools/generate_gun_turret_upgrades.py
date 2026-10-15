"""Author textured turret upgrade tiers in a fresh background Blender.

Preserves the original two render meshes, collider, sockets and their transforms.
Replaces only upgrade_* meshes; two complete overlays per selected level.
"""
from pathlib import Path
import copy, hashlib, importlib.util, json, math, struct, tempfile
import bpy
from mathutils import Matrix, Vector

assert bpy.app.background, 'Run in a separate background Blender'
bpy.ops.wm.read_factory_settings(use_empty=True)
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/gun_turret_levels';ART.mkdir(parents=True,exist_ok=True)
OUTPUT=ROOT/'assets/models/buildings/gun_turret.glb'
BASE=ART/'base_reference.glb'
PALETTE=ROOT/'assets/models/buildings/Textures/colormap.png'

def read_glb(path):
    data=path.read_bytes();n=struct.unpack_from('<I',data,12)[0]
    return json.loads(data[20:20+n]),data[28+n:]
def write_glb(path,doc,binary):
    doc['buffers'][0]['byteLength']=len(binary)
    js=json.dumps(doc,separators=(',',':')).encode();js+=b' '*(-len(js)%4)
    binary+=b'\0'*(-len(binary)%4)
    path.write_bytes(struct.pack('<III',0x46546c67,2,28+len(js)+len(binary))+
        struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(binary),0x004e4942)+binary)

if not BASE.exists():
    doc,binary=read_glb(OUTPUT)
    first=next(i for i,m in enumerate(doc['meshes']) if m.get('name','').startswith('upgrade_'))
    a=doc['meshes'][first]['primitives'][0]['attributes']['POSITION'];v=doc['accessors'][a]['bufferView']
    cutoff=doc['bufferViews'][v]['byteOffset']
    node=next(i for i,n in enumerate(doc['nodes']) if n.get('mesh')==first)
    doc['meshes']=doc['meshes'][:first];doc['nodes']=doc['nodes'][:node]
    doc['accessors']=doc['accessors'][:a];doc['bufferViews']=doc['bufferViews'][:v];doc['materials']=doc['materials'][:1]
    for scene in doc['scenes']:scene['nodes']=[i for i in scene['nodes'] if i<node]
    write_glb(BASE,doc,binary[:cutoff])

spec=importlib.util.spec_from_file_location('turret_family_geometry',ROOT/'tools/create_tesla_upgrades.py')
family=importlib.util.module_from_spec(spec);spec.loader.exec_module(family)
Geometry=family.Geometry
GOLD=(.65625,.285,.465);ORANGE=(.84375,.285,.465)
RED=(.96875,.285,.465);DARK=(.59375,.035,.215);STEEL=(.84375,.035,.215)
scene=family.new_scene('GUN_TURRET_LEVELS');studio=family.studio(scene)
collection=bpy.data.collections.new('TURRET_UPGRADE_MODEL');scene.collection.children.link(collection)
source_collection=bpy.data.collections.new('TURRET_BASE_REFERENCE');scene.collection.children.link(source_collection)
before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(BASE));bpy.context.view_layer.update()
source=list(set(scene.objects)-before)
for obj in source:
    for c in list(obj.users_collection):c.objects.unlink(obj)
    source_collection.objects.link(obj)
    if obj.name.startswith('COL_'):obj.hide_render=True;obj.display_type='WIRE'
image=bpy.data.images.load(str(PALETTE),check_existing=False);image.pack()
material=bpy.data.materials.new('turret_upgrade_colormap');material.use_nodes=True
shader=next(n for n in material.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Roughness'].default_value=1;shader.inputs['Metallic'].default_value=0
tex=material.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
material.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])

def side_plate(g,name,outline,side,lo,hi,swatch):
    # GLB (height,depth) outline, extruded along GLB X; convert to Blender.
    start=len(g.verts)
    g.prism(name,[(y,-z) for y,z in outline],(hi-lo)/2,0,swatch)
    for i in range(start,len(g.verts)):
        height,tangent,depth=g.verts[i]
        g.verts[i]=(side*((lo+hi)/2+tangent),depth,height)

def tube_ring(g,name,x,front,back,inner,outer,swatch):
    start=len(g.verts);g.ring(name,-back,-front,inner,outer,swatch)
    pose=Matrix.Translation((x,0,.685445))@Matrix.Rotation(-math.pi/2,4,'X')
    for i in range(start,len(g.verts)):g.verts[i]=tuple(pose@Vector(g.verts[i]))

def barrel_guard(g,name,x):
    # One continuous hollow steel/gold sleeve. Adjacent rings must not leave
    # duplicate internal caps or welded non-manifold edges at the color seam.
    profile=[(.105,-.477),(.136,-.477),(.136,-.633),
             (.138,-.633),(.138,-.665),(.105,-.665)]
    bands=[STEEL,STEEL,GOLD,GOLD,GOLD,DARK];sides=12
    verts=[(x+r*math.cos(math.tau*j/sides),-depth,.685445+r*math.sin(math.tau*j/sides))
           for r,depth in profile for j in range(sides)]
    faces=[];smooth=[]
    for band in range(len(profile)):
        following=(band+1)%len(profile)
        for j in range(sides):
            k=(j+1)%sides
            faces.append((band*sides+j,band*sides+k,following*sides+k,following*sides+j))
            if profile[band][1]!=profile[following][1]:smooth.append(len(faces)-1)
    first=len(g.faces);g.add(name,verts,faces,STEEL,smooth)
    for band,swatch in enumerate(bands):
        g.swatches[first+band*sides:first+(band+1)*sides]=[swatch]*sides

def collar(g,name,lo,hi,swatch,thickness=.025):
    def r(z):
        return (.40+(.32-.40)*(z-.15)/.15 if z<=.30 else .32+(.211761-.32)*(z-.30)/.16)+.004
    g.lathe(name,[(r(lo),lo),(r(lo)+thickness,lo),(r(hi)+thickness,hi),(r(hi),hi)],swatch)

levels={}
for level in range(2,9):
    base,head=Geometry(),Geometry()
    collar(base,'gold_pedestal_belt',.218,.274,GOLD)
    if level>=3:
        for i in range(4):
            a=math.pi/4+i*math.pi/2
            base.prism('armored_foot_'+str(i),[(.325,.17),(.405,.17),(.405,.205),(.300,.376),(.264,.376),(.285,.31)],.050,a,GOLD)
    if level>=5:collar(base,'upper_pedestal_belt',.348,.382,GOLD,.023)
    if level==8:collar(base,'master_foot_belt',.160,.200,GOLD,.032)
    cheek=[(.49,-.28),(.51,.39),(.86,.43),(.98,.28),(.94,-.17),(.77,-.34)]
    inset=[(.55,-.205),(.57,.33),(.825,.355),(.915,.245),(.88,-.125),(.75,-.265)]
    for sign in (-1,1):
        side_plate(head,'gold_housing_cheek_'+str(sign),cheek,sign,.418,.458,GOLD)
        if level>=3:side_plate(head,'orange_armor_inset_'+str(sign),inset,sign,.4585,.474,ORANGE)
        if level>=4:
            # Bearing caps on the rotating housing; never attached to the foot.
            start=len(head.verts)
            head.lathe('side_bearing_'+str(sign),[(.047,.467),(.065,.467),(.065,.491),(.053,.506)],DARK,sides=8)
            pose=Matrix.Translation((0,-.015,.728))@Matrix.Rotation(sign*math.pi/2,4,'Y')
            for i in range(start,len(head.verts)):head.verts[i]=tuple(pose@Vector(head.verts[i]))
    if level>=4:
        lift=.075 if level>=7 else 0
        roof=[(1.01,-.28),(1.01,.38),(1.065+lift,.32),(1.10+lift,.18),(1.10+lift,-.10),(1.06+lift,-.26)]
        side_plate(head,'gold_roof_armor',roof,1,-.335,.335,GOLD)
        insert=[(1.069+lift,-.24),(1.069+lift,.28),(1.106+lift,.17),(1.106+lift,-.10)]
        side_plate(head,'orange_roof_inset',insert,1,-.225,.225,ORANGE)
    if level>=5:
        # Twin rear magazines change the silhouette without adding fake barrels.
        for sign in (-1,1):
            center=(sign*.22,-.444)
            head.lathe('magazine_mount_'+str(sign),[(.045,.60),(.098,.60),(.098,.646),(.084,.665)],DARK,center,8)
            head.lathe('rear_magazine_'+str(sign),[(.065,.638),(.085,.658),(.085,.92),(.068,.945)],RED,center,8)
            head.ring('magazine_lower_band_'+str(sign),.662,.690,.081,.095,GOLD,center,8)
            head.lathe('magazine_gold_lid_'+str(sign),[(.050,.922),(.094,.922),(.094,.953),(.076,.969)],GOLD,center,8)
    if level>=6:
        for x in (-.140388,.140388):
            barrel_guard(head,'continuous_barrel_guard_'+str(x),x)
    if level>=7:
        # Stout rear bridge ties the roof into the magazine assembly.
        bridge=[(.94,.36),(.94,.52),(1.02,.56),(1.095,.49),(1.095,.34)]
        side_plate(head,'rear_gold_bridge',bridge,1,-.30,.30,GOLD)
        panel=[(.978,.395),(.978,.495),(1.023,.521),(1.068,.475),(1.068,.385)]
        side_plate(head,'bridge_orange_center',panel,1,-.21,.21,ORANGE)
    if level==8:
        for x in (-.140388,.140388):
            tube_ring(head,'master_breech_collar_'+str(x),x,-.485,-.442,.103,.138,GOLD)
            tube_ring(head,'warm_barrel_band_'+str(x),x,-.601,-.570,.134,.138,ORANGE)
    objects=[]
    for role,g in [('base',base),('head',head)]:
        obj=g.object(f'upgrade_l{level}_{role}',collection,material)
        obj['level']=level;obj['rig_group']=role;obj.hide_render=True;objects.append(obj)
    levels[level]=objects

# Export only the overlays, then merge their geometry into the preserved asset.
# Original accessors/nodes/material/texture bytes stay intact; runtime order stays 16.
bpy.ops.object.select_all(action='DESELECT')
for objects in levels.values():
    for obj in objects:obj.select_set(True)
bpy.context.view_layer.objects.active=levels[8][0]
with tempfile.TemporaryDirectory(prefix='ian-turret-overlays-') as temp:
    path=Path(temp)/'overlays.glb'
    bpy.ops.export_scene.gltf(filepath=str(path),use_selection=True,use_active_scene=True,
        use_visible=False,use_renderable=False,export_yup=True,export_extras=True,export_animations=False)
    extra,extra_binary=read_glb(path)
    doc,original_binary=read_glb(BASE);binary=bytearray(original_binary[:doc['buffers'][0]['byteLength']])
    by_name={m['name']:m for m in extra['meshes']}
    bounds=[];triangles=[]
    for level in range(2,9):
        for role in ('base','head'):
            name=f'upgrade_l{level}_{role}';primitive=by_name[name]['primitives'][0]
            remap={}
            for old in list(primitive['attributes'].values())+[primitive['indices']]:
                if old in remap:continue
                a=copy.deepcopy(extra['accessors'][old]);view=copy.deepcopy(extra['bufferViews'][a['bufferView']])
                start=view.get('byteOffset',0);binary.extend(b'\0'*(-len(binary)%4))
                view['byteOffset']=len(binary);binary.extend(extra_binary[start:start+view['byteLength']])
                view['buffer']=0;a['bufferView']=len(doc['bufferViews']);doc['bufferViews'].append(view)
                remap[old]=len(doc['accessors']);doc['accessors'].append(a)
            p={'attributes':{key:remap[v] for key,v in primitive['attributes'].items()},'indices':remap[primitive['indices']],'material':0}
            a=extra['accessors'][primitive['attributes']['POSITION']];bounds.append([a['min'],a['max']])
            triangles.append(extra['accessors'][primitive['indices']]['count']//3)
            index=len(doc['meshes']);doc['meshes'].append({'name':name,'primitives':[p]})
            node=len(doc['nodes']);doc['nodes'].append({'name':name,'mesh':index,'extras':{'level':level,'rig_group':role}})
            doc['scenes'][doc.get('scene',0)]['nodes'].append(node)
    write_glb(OUTPUT,doc,bytes(binary))

# Keep the established renderer interface; regenerate only accurate local bounds.
header=(ROOT/'src/graphics/GunTurretModel.hpp').read_text()
a=header.index('inline constexpr std::array<BoundingBox, 14> UpgradeBounds{{')
b=header.index('}};',a)
vec=lambda v:'{'+', '.join(f'{x:.6f}F' for x in v)+'}'
rows=''.join('    {'+vec(lo)+', '+vec(hi)+'},\n' for lo,hi in bounds)
header=header[:a]+'inline constexpr std::array<BoundingBox, 14> UpgradeBounds{{\n'+rows+header[b:]
(ROOT/'src/graphics/GunTurretModel.hpp').write_text(header)
manifest={'runtime_glb':str(OUTPUT.relative_to(ROOT)),'base_reference_sha256':hashlib.sha256(BASE.read_bytes()).hexdigest(),
    'palette_sha256':hashlib.sha256(PALETTE.read_bytes()).hexdigest(),'render_mesh_count':16,'raw_mesh_count':17,
    'materials':1,'additional_draws':2,'upgrade_mode':'L1 original; L2-L8 original plus current complete base/head pair',
    'mesh_order':'base 0, housing 1 after collider removal, then base/head pairs L2-L8',
    'bounds_glb':bounds,'overlay_triangles':triangles,'source_nodes':doc['nodes'][:7],
    'pivot_glb':[0,0,0],'axes':'Y-up GLB; -Z front; original head geometry has its authored node offset baked by loader',
    'motion':'base follows placement yaw; head follows aim yaw and horizontal recoil; no pitch',
    'sockets':'Original Muzzle nodes and collider byte-identical. Hollow forward collars leave both shot channels unobstructed.',
    'license':'Kenney base/palette CC0; new upgrade geometry project-owned','native_playtest':False}
def glb_bounds(objects):
    coords=[(p.x,p.z,-p.y) for obj in objects for vertex in obj.data.vertices
            for p in [obj.matrix_world@vertex.co]]
    return [[min(p[i] for p in coords) for i in range(3)],
            [max(p[i] for p in coords) for i in range(3)]]
bpy.context.view_layer.update()
original_visible=[obj for obj in source if obj.type=='MESH' and not obj.name.startswith('COL_')]
manifest['visible_bounds_glb']={str(level):glb_bounds(original_visible+levels.get(level,[])) for level in range(1,9)}
manifest['collision_bounds_glb']=glb_bounds([obj for obj in source if obj.type=='MESH' and obj.name.startswith('COL_')])
manifest['footprint_m']=[2,2]
(ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')

# Rig controls in the editable source rotate whole mechanical groups, preserving
# world-space positions. Runtime continues using its established renderer path.
yaw=bpy.data.objects.new('turret_yaw_control',None);collection.objects.link(yaw)
for obj in source:
    if obj.name=='barrel':
        world=obj.matrix_world.copy();obj.parent=yaw;obj.matrix_world=world
for objects in levels.values():
    obj=objects[1];world=obj.matrix_world.copy();obj.parent=yaw;obj.matrix_world=world
bpy.context.view_layer.update()
scene.view_settings.exposure=-.25;scene.render.resolution_x=600;scene.render.resolution_y=600
camera=scene.camera

def render(level,label='beauty'):
    for current,objects in levels.items():
        for obj in objects:obj.hide_render=current!=level;obj.hide_set(current!=level)
    views={'beauty':((2.5,4,2.7),(0,0,.62),1.8),'front':((0,5,.62),(0,0,.62),1.65),
        'side':((5,0,.62),(0,0,.62),1.65),'back':((0,-5,.62),(0,0,.62),1.65),'top':((0,.001,5),(0,0,0),1.65)}
    pos,target,scale=views[label];camera.location=pos;family.aim(camera,target);camera.data.ortho_scale=scale
    scene.render.filepath=str(ART/f'level_{level}_{label}.png');bpy.ops.render.render(write_still=True)
for level in range(1,9):render(level)
for view in ('front','side','back','top'):render(8,view)
render(8)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'gun_turret_levels.blend'),check_existing=False)

comparison=family.new_scene('GUN_TURRET_FAMILY');family.studio(comparison)
family.import_visible(ROOT/'assets/models/buildings/flamethrower.glb',offset=-1.6)
family.import_visible(ROOT/'assets/models/buildings/flamethrower_upgrades.glb',lambda o:'_l8_' in o.name,offset=-1.6)
family.import_visible(OUTPUT,lambda o:not o.name.startswith('upgrade_') or '_l8_' in o.name)
family.import_visible(ROOT/'assets/models/buildings/tesla_tower.glb',offset=1.6)
family.import_visible(ROOT/'assets/models/buildings/tesla_upgrades.glb',lambda o:'l8' in o.name,offset=1.6)
comparison.camera.location=(2,9,4.5);family.aim(comparison.camera,(0,0,.7));comparison.camera.data.ortho_scale=5.2
comparison.render.resolution_x=1500;comparison.render.resolution_y=700
comparison.render.filepath=str(ART/'family_comparison.png');bpy.ops.render.render(write_still=True)
print('TURRET_LEVELS_WRITTEN',json.dumps({'overlay_triangles':triangles,'render_meshes':16}))

"""Author textured crossbow upgrade tiers in a fresh background Blender.

Preserves the original two render meshes, collider, socket and their transforms.
Replaces only upgrade_* meshes; two complete overlays per selected level.
"""
from pathlib import Path
import copy, hashlib, importlib.util, json, math, struct, tempfile
import bpy
from mathutils import Matrix, Vector

assert bpy.app.background, 'Run in a separate background Blender'
bpy.ops.wm.read_factory_settings(use_empty=True)
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/crossbow_levels';ART.mkdir(parents=True,exist_ok=True)
OUTPUT=ROOT/'assets/models/weapons/crossbow.glb'
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

spec=importlib.util.spec_from_file_location('crossbow_family_geometry',ROOT/'tools/create_tesla_upgrades.py')
family=importlib.util.module_from_spec(spec);spec.loader.exec_module(family)
Geometry=family.Geometry
GOLD=(.65625,.285,.465);ORANGE=(.84375,.285,.465)
DARK=(.59375,.035,.215)
scene=family.new_scene('CROSSBOW_LEVELS');studio=family.studio(scene)
collection=bpy.data.collections.new('CROSSBOW_UPGRADE_MODEL');scene.collection.children.link(collection)
source_collection=bpy.data.collections.new('CROSSBOW_BASE_REFERENCE');scene.collection.children.link(source_collection)
before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(BASE));bpy.context.view_layer.update()
source=list(set(scene.objects)-before)
for obj in source:
    for c in list(obj.users_collection):c.objects.unlink(obj)
    source_collection.objects.link(obj)
    if obj.name.startswith('COL_'):obj.hide_render=True;obj.display_type='WIRE'
image=bpy.data.images.load(str(PALETTE),check_existing=False);image.pack()
material=bpy.data.materials.new('crossbow_upgrade_colormap');material.use_nodes=True
shader=next(n for n in material.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Roughness'].default_value=1;shader.inputs['Metallic'].default_value=0
tex=material.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
material.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])

def horizontal_prism(g,name,outline,lo,hi,swatch):
    # Preserve the concave swept bow opening; Blender triangulates the end
    # polygons, rather than filling it with a triangle fan.
    n=len(outline)
    verts=[(x,-depth,height) for height in (lo,hi) for x,depth in outline]
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
    faces.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
    g.add(name,verts,faces,swatch)

def side_plate(g,name,outline,side,lo,hi,swatch):
    start=len(g.verts);g.prism(name,[(y,-z) for y,z in outline],(hi-lo)/2,0,swatch)
    for i in range(start,len(g.verts)):
        height,tangent,depth=g.verts[i];g.verts[i]=(side*((lo+hi)/2+tangent),depth,height)

def side_frame(g,name,outer,inner,side,lo,hi,swatch):
    # A real open frame, with a through-cut center. A second filled plate
    # would cover the orange inset when viewed from either side.
    n=len(outer);assert len(inner)==n
    verts=[(side*x,-depth,height) for x,contour in ((lo,outer),(hi,outer),(lo,inner),(hi,inner))
           for height,depth in contour]
    faces=[]
    for i in range(n):
        j=(i+1)%n
        faces.extend([(i,j,n+j,n+i),(2*n+i,3*n+i,3*n+j,2*n+j),
                      (i,2*n+i,2*n+j,j),(n+i,n+j,3*n+j,3*n+i)])
    g.add(name,verts,faces,swatch)

def stock_binding(g,name,bottom,top,front,back,width,swatch):
    bevel=.018
    outline=[(-width/2+bevel,front),(width/2-bevel,front),(width/2,front+bevel),
             (width/2,back-bevel),(width/2-bevel,back),(-width/2+bevel,back),
             (-width/2,back-bevel),(-width/2,front+bevel)]
    horizontal_prism(g,name,outline,bottom,top,swatch)

def axle(g,name,side,lo,hi,inner,outer,swatch):
    start=len(g.verts);g.ring(name,lo,hi,inner,outer,swatch,sides=8)
    pose=Matrix.Translation((0,-.445,.836))@Matrix.Rotation(side*math.pi/2,4,'Y')
    for i in range(start,len(g.verts)):g.verts[i]=tuple(pose@Vector(g.verts[i]))

outline=[(-.690,-.087),(-.449,-.291),(-.237,-.410),(0,-.452),
         (.237,-.410),(.449,-.291),(.690,-.087),(.577,.066),
         (.340,-.123),(.181,-.210),(0,-.236),(-.181,-.210),(-.340,-.123),(-.577,.066)]
outer=outline[:7]
levels={}
for level in range(2,9):
    base,bow=Geometry(),Geometry()
    base.ring('gold_foundation_belt',.065,.125,.550,.605,GOLD)
    stock_binding(bow,'rear_stock_binding',.735,.865,.2925,.4275,.225,GOLD)
    if level>=3:
        cheek=[(.21,-.38),(.21,.30),(.70,.30),(.83,.08),(.67,-.27),(.45,-.40)]
        cheek=[(.21+(y-.21)*.72,z) for y,z in cheek]
        inset=[(.25,-.31),(.25,.235),(.565,.235),(.608,.07),(.51,-.24),(.395,-.32)]
        for side in (-1,1):
            side_plate(base,'gold_carriage_armor_'+str(side),cheek,side,.480,.615,GOLD)
            side_plate(base,'orange_carriage_inset_'+str(side),inset,side,.6155,.630,ORANGE)
    width=1.07 if level>=7 else 1.0
    plan=[(x*width,z) for x,z in outline]
    if level>=4:
        front_rail=[(x*width,z-.012) for x,z in outer]+[(x*width,z+.025) for x,z in reversed(outer)]
        horizontal_prism(bow,'gold_lower_bow_rail',front_rail,.770,.810,GOLD)
    if level>=5:
        # Four separate ferrules leave the wooden span, string and stock visible.
        # They are clamped to the actual bow, not tall continuous orange walls.
        for i in (0,2,3,5):
            def mix(a,b,t):return (a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t)
            a,b=outer[i],outer[i+1]
            p,q=mix(a,b,.24),mix(a,b,.64)
            strip=[(p[0],p[1]-.018),(q[0],q[1]-.018),
                   (q[0],q[1]+.012),(p[0],p[1]+.012)]
            strip=[(x*width,z) for x,z in strip]
            horizontal_prism(bow,'gold_limb_ferrule_'+str(i),strip,.770,.982,GOLD)
    if level>=6:
        base.ring('upper_foundation_trim',.128,.153,.568,.613,GOLD)
        # A stock-mounted winch moves with the pitched assembly.
        for side in (-1,1):
            axle(bow,'winch_gold_shoulder_'+str(side),side,.120,.180,.016,.080,GOLD)
            axle(bow,'winch_dark_hub_'+str(side),side,.181,.215,.016,.056,DARK)
            axle(bow,'winch_outer_nut_'+str(side),side,.216,.243,.015,.041,GOLD)
    if level>=7:
        horizontal_prism(bow,'master_upper_bow_rail',plan,.941,1.038,GOLD)
    if level==8:
        # A thin inset on the front face of the gold rail; no coplanar skin.
        stripe=[(x*width,z-.007) for x,z in outer]+[(x*width,z+.010) for x,z in reversed(outer)]
        horizontal_prism(bow,'orange_bow_face_inset',stripe,.972,1.014,ORANGE)
        base.ring('master_foundation_trim',.157,.182,.569,.623,GOLD)
        for side in (-1,1):
            outer_frame=[(.42+(y-.42)*.96,-.04+(z+.04)*.96) for y,z in cheek]
            open_center=[(.42+(y-.42)*.90,-.04+(z+.04)*.90) for y,z in inset]
            side_frame(base,'open_master_carriage_frame_'+str(side),outer_frame,open_center,side,.628,.650,GOLD)
    objects=[]
    for role,g in [('base',base),('bow',bow)]:
        obj=g.object(f'upgrade_crossbow_l{level}_{role}',collection,material)
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
        for role in ('base','bow'):
            name=f'upgrade_crossbow_l{level}_{role}';primitive=by_name[name]['primitives'][0]
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

# Regenerate bounds, retaining the established render/aiming interface.
header=(ROOT/'src/graphics/CrossbowModel.hpp').read_text()
a=header.index('inline constexpr std::array<BoundingBox, 14> UpgradeBounds{{')
b=header.index('}};',a)
vec=lambda v:'{'+', '.join(f'{x:.6f}F' for x in v)+'}'
rows=''.join('    {'+vec(lo)+', '+vec(hi)+'},\n' for lo,hi in bounds)
header=header[:a]+'inline constexpr std::array<BoundingBox, 14> UpgradeBounds{{\n'+rows+header[b:]
(ROOT/'src/graphics/CrossbowModel.hpp').write_text(header)
manifest={'runtime_glb':str(OUTPUT.relative_to(ROOT)),'base_reference_sha256':hashlib.sha256(BASE.read_bytes()).hexdigest(),
    'palette_sha256':hashlib.sha256(PALETTE.read_bytes()).hexdigest(),'render_mesh_count':16,'raw_mesh_count':17,
    'materials':1,'additional_draws':2,'upgrade_mode':'L1 original; L2-L8 original plus current complete carriage/bow pair',
    'mesh_order':'pitched bow 0, yaw carriage 1 after collider removal, then base/bow pairs L2-L8',
    'bounds_glb':bounds,'overlay_triangles':triangles,'pitch_pivot_glb':[0,.14931100606918335,0],
    'axes':'Y-up GLB, -Z front; Blender +Z up, +Y front',
    'motion':'Carriage yaw; bow follows original pitch pivot, then yaw. Original source geometry and sockets unchanged.',
    'license':'Kenney base/palette CC0; new upgrade geometry project-owned','native_playtest':False}
def glb_bounds(objects):
    coords=[(p.x,p.z,-p.y) for obj in objects for vertex in obj.data.vertices for p in [obj.matrix_world@vertex.co]]
    return [[min(p[i] for p in coords) for i in range(3)],[max(p[i] for p in coords) for i in range(3)]]
bpy.context.view_layer.update()
original_visible=[obj for obj in source if obj.type=='MESH' and not obj.name.startswith('COL_')]
manifest['visible_bounds_glb']={str(level):glb_bounds(original_visible+levels.get(level,[])) for level in range(1,9)}
manifest['collision_bounds_glb']=glb_bounds([obj for obj in source if obj.type=='MESH' and obj.name.startswith('COL_')])
manifest['footprint_m']=[2,2]
(ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')

# Use the actual original Blender pivots and preserve world transforms on parenting.
yaw=next(obj for obj in source if obj.name.startswith('yaw_pivot'))
pitch=next(obj for obj in source if obj.name=='pitch_pivot')
yaw.rotation_mode='XYZ';pitch.rotation_mode='XYZ'
for objects in levels.values():
    for obj in objects:
        world=obj.matrix_world.copy();obj.parent=pitch if obj['rig_group']=='bow' else yaw
        bpy.context.view_layer.update();obj.matrix_world=world
bpy.context.view_layer.update()
scene.view_settings.exposure=-.25;scene.render.resolution_x=600;scene.render.resolution_y=600
camera=scene.camera

def render(level,label='beauty'):
    for current,objects in levels.items():
        for obj in objects:obj.hide_render=current!=level;obj.hide_set(current!=level)
    views={'beauty':((2.5,4,2.7),(0,0,.62),2.1),'front':((0,5,.62),(0,0,.62),2.0),
        'side':((5,0,.62),(0,0,.62),2.0),'back':((0,-5,.62),(0,0,.62),2.0),'top':((0,.001,5),(0,0,0),2.0)}
    pos,target,scale=views[label];camera.location=pos;family.aim(camera,target);camera.data.ortho_scale=scale
    scene.render.filepath=str(ART/f'level_{level}_{label}.png');bpy.ops.render.render(write_still=True)
for level in range(1,9):render(level)
for view in ('front','side','back','top'):render(8,view)
render(8)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'crossbow_levels.blend'),check_existing=False)

comparison=family.new_scene('CROSSBOW_FAMILY');family.studio(comparison)
family.import_visible(ROOT/'assets/models/buildings/flamethrower.glb',offset=-1.6)
family.import_visible(ROOT/'assets/models/buildings/flamethrower_upgrades.glb',lambda o:'_l8_' in o.name,offset=-1.6)
family.import_visible(OUTPUT,lambda o:not o.name.startswith('upgrade_') or '_l8_' in o.name)
family.import_visible(ROOT/'assets/models/buildings/tesla_tower.glb',offset=1.6)
family.import_visible(ROOT/'assets/models/buildings/tesla_upgrades.glb',lambda o:'l8' in o.name,offset=1.6)
comparison.camera.location=(2,9,4.5);family.aim(comparison.camera,(0,0,.7));comparison.camera.data.ortho_scale=5.2
comparison.render.resolution_x=1500;comparison.render.resolution_y=700
comparison.render.filepath=str(ART/'family_comparison.png');bpy.ops.render.render(write_still=True)
print('CROSSBOW_LEVELS_WRITTEN',json.dumps({'overlay_triangles':triangles,'render_meshes':16}))

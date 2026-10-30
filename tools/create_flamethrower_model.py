"""Build a palette-textured flamethrower in the supplied Kenney weapon style.

Run with Blender Python. Preserves existing scenes and original reference files.
Run: Blender --factory-startup --background --python tools/create_flamethrower_model.py
The editable project and runtime GLB use final metre units, +Y forward in
Blender and -Z in glTF. The optional reference GLB is half-size. No VFX baked in.
"""
from pathlib import Path
import json
import math
import subprocess
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models'
VIEWS = ART / 'flamethrower_views'
VIEWS.mkdir(parents=True, exist_ok=True)
scene = bpy.data.scenes.new('Flamethrower')
scene['generated_model'] = 'flamethrower'
bpy.context.window.scene = scene
model = bpy.data.collections.new('FLAMETHROWER_MODEL')
scene.collection.children.link(model)
studio = bpy.data.collections.new('FLAMETHROWER_STUDIO')
scene.collection.children.link(studio)
collision = bpy.data.collections.new('FLAMETHROWER_COLLISION')
scene.collection.children.link(collision)
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1

# Share the actual kit image and the same diffuse-only material recipe.
image = bpy.data.images.load(str(ROOT / 'assets/models/buildings/Textures/colormap.png'), check_existing=False)
image.name = 'flamethrower_colormap.png'
image.pack()
palette = bpy.data.materials.new('flamethrower_colormap')
palette.use_nodes = True
shader = next(n for n in palette.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value = 0
shader.inputs['Roughness'].default_value = 1
texture = palette.node_tree.nodes.new('ShaderNodeTexImage')
texture.image = image
palette.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])

# Column centers and vertical swatch ranges from the original UVs.
WOOD = (.09375, .275, .475)
LIGHT_WOOD = (.34375, .275, .475)
STEEL = (.84375, .025, .225)
DARK_STEEL = (.59375, .025, .225)
CHARCOAL = (.46875, .025, .225)
ORANGE = (.84375, .275, .475)
TANK_RED = (.96875, .275, .475)

def mesh_object(name, vertices, faces, swatch, smooth_faces=()):
    mesh = bpy.data.meshes.new(name)
    # Author the front at +Y. This is
    # geometry orientation; the root and all pivot axes remain neutral.
    mesh.from_pydata([(-x, -y, z) for x, y, z in vertices], [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    model.objects.link(obj)
    mesh.materials.append(palette)
    uv = mesh.uv_layers.new(name='UVMap')
    low = min(v.co.z for v in mesh.vertices)
    high = max(v.co.z for v in mesh.vertices)
    for poly in mesh.polygons:
        poly.use_smooth = poly.index in smooth_faces
        for li in poly.loop_indices:
            z = mesh.vertices[mesh.loops[li].vertex_index].co.z
            t = (z-low)/max(high-low, 1e-5)
            uv.data[li].uv = (swatch[0], swatch[1]+t*(swatch[2]-swatch[1]))
    mesh.set_sharp_from_angle(angle=math.radians(45))
    return obj

def lathe(name, profile, swatch, segments=12, axis='Z', center=(0,0,0), smooth=True):
    # profile is (radius, axial position), with outward surface winding.
    vertices=[]
    for r,z in profile:
        for i in range(segments):
            a=math.tau*i/segments
            co=Vector((r*math.cos(a),r*math.sin(a),z))
            if axis=='Y': co=Vector((co.x,-co.z,co.y))
            if axis=='X': co=Vector((co.z,co.y,-co.x))
            vertices.append(tuple(co+Vector(center)))
    faces=[]
    smooth_faces=[]
    for ring in range(len(profile)-1):
        vertical=abs(profile[ring+1][1]-profile[ring][1])>1e-6
        for i in range(segments):
            a=ring*segments+i; b=ring*segments+(i+1)%segments
            if smooth and vertical: smooth_faces.append(len(faces))
            faces.append((a,b,b+segments,a+segments))
    faces += [tuple(reversed(range(segments))), tuple((len(profile)-1)*segments+i for i in range(segments))]
    return mesh_object(name,vertices,faces,swatch,smooth_faces)

def prism(name, outline_yz, xlo, xhi, swatch):
    n=len(outline_yz)
    verts=[(x,y,z) for x in (xlo,xhi) for y,z in outline_yz]
    faces=[tuple(reversed(range(n))),tuple(n+i for i in range(n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    return mesh_object(name,verts,faces,swatch)

def empty(name,loc,parent=None):
    o=bpy.data.objects.new(name,None);model.objects.link(o)
    o.location=loc;o.parent=parent
    o.empty_display_type='PLAIN_AXES';o.empty_display_size=.035
    return o

root=empty('asset_root',(0,0,0))
root['source_style']='Kenney Tower Defense Kit 2.1'
root['palette']='Original Textures/colormap.png; unchanged'
root['forward_axis']='Blender +Y / glTF -Z'
root['model_units']='Runtime metres: base diameter 1.2'
base_parts=[]
mount_parts=[]
head_parts=[]

# Identical 12-sided profile to the common cannon/catapult/ballista foot.
base_parts.append(lathe('flamethrower_base',[(.19647,0),(.19647,.05),(.30,.05),(.30,.15)],WOOD))
base_parts.append(lathe('flamethrower_pedestal',[(.18,.145),(.18,.17),(.154,.29),(.139,.305)],ORANGE))
# Four broad tapered silver braces echo the existing building mounts.
for i in range(4):
    brace=prism('flamethrower_base_brace_%d'%i,[(-.055,.145),(.055,.145),(.040,.295),(.022,.317),(-.022,.317),(-.040,.295)],.142,.205,STEEL)
    # The support flares radially towards the bottom, like the supplied assets.
    for v in brace.data.vertices:
        v.co.x -= (.30-v.co.z)*.22
    brace.rotation_euler.z=math.pi/4+i*math.pi/2
    base_parts.append(brace)
    bolt=lathe('flamethrower_base_bolt_%d'%i,[(.024,.215),(.024,.228)],DARK_STEEL,segments=8,axis='X',center=(0,0,.215),smooth=False)
    bolt.rotation_euler.z=brace.rotation_euler.z
    base_parts.append(bolt)

yaw=empty('yaw_pivot',(0,0,.312),root)
mount_parts.append(lathe('flamethrower_turntable',[(.13,.302),(.141,.309),(.141,.329),(.126,.336)],DARK_STEEL))
mount_parts.append(lathe('flamethrower_yoke_foot',[(.103,.330),(.103,.343),(.089,.350)],LIGHT_WOOD))
# Wooden rounded-top clevis and steel pivot bolts, kept intentionally chunky.
for sign in (-1,1):
    xs=sorted((sign*.084,sign*.122))
    mount_parts.append(prism('flamethrower_yoke_%s'%sign,[(-.047,.348),(.060,.348),(.043,.480),(.026,.497),(-.026,.497),(-.047,.478)],xs[0],xs[1],LIGHT_WOOD))
    center_x=sign*.123
    bolt=lathe('flamethrower_pitch_bolt_%s'%sign,[(.025,-.008),(.025,.008)],DARK_STEEL,8,'X',(center_x,0,.462),False)
    mount_parts.append(bolt)

pitch=empty('pitch_pivot',(0,0,.150),yaw)
# Axial position increases towards the muzzle; Y transform points along -Y.
# Extend the rear housing towards the reservoir; taper the new tail so it
# clears the wooden foot when the muzzle is raised to +30 degrees.
head_parts.append(lathe('flamethrower_rear_receiver',[(.043,-.160),(.060,-.095),(.076,-.073),(.076,.065),(.064,.090)],DARK_STEEL,10,'Y',(0,0,.462)))
head_parts.append(lathe('flamethrower_receiver_band',[(.079,-.019),(.079,.022)],STEEL,10,'Y',(0,0,.462)))
head_parts.append(lathe('flamethrower_heat_shroud',[(.068,.079),(.081,.096),(.081,.200),(.067,.219)],ORANGE,10,'Y',(0,0,.462)))
head_parts.append(lathe('flamethrower_nozzle_neck',[(.057,.212),(.057,.242)],CHARCOAL,10,'Y',(0,0,.462)))
# Closed recessed nozzle: the outer lip folds into the dark interior throat.
head_parts.append(lathe('flamethrower_nozzle',[(.068,.237),(.075,.245),(.075,.276),(.068,.285),(.040,.285),(.040,.246)],DARK_STEEL,10,'Y',(0,0,.462)))
head_parts.append(lathe('flamethrower_nozzle_throat',[(.040,.244),(.040,.247)],CHARCOAL,10,'Y',(0,0,.462),False))
# A tiny pilot nozzle below the main mouth reads at game camera distances.
head_parts.append(lathe('flamethrower_pilot',[(.016,.230),(.016,.281)],CHARCOAL,8,'Y',(0,0,.388)))

# The reservoir follows yaw but stays upright during pitch. Leave clearance
# behind the receiver throughout the -10..+30 degree elevation envelope.
mount_parts.append(lathe('flamethrower_fuel_tank',[(.059,.305),(.079,.326),(.079,.523),(.062,.549)],TANK_RED,10,center=(0,.285,0)))
mount_parts.append(lathe('flamethrower_tank_cap',[(.046,.544),(.046,.560),(.034,.570)],DARK_STEEL,8,center=(0,.285,0)))
mount_parts.append(lathe('flamethrower_tank_boot',[(.043,.285),(.057,.298),(.057,.318)],DARK_STEEL,8,center=(0,.285,0)))
mount_parts.append(lathe('flamethrower_tank_band',[(.086,.397),(.086,.435)],DARK_STEEL,10,center=(0,.285,0)))
mount_parts.append(prism('flamethrower_tank_bracket',[(.06,.321),(.285,.321),(.285,.340),(.06,.340)],-.035,.035,STEEL))

# Short six-sided hose with two broad bends, no fine realistic greebles.
def hose(name,points,radius=.016,segments=6):
    vertices=[]
    for i,p in enumerate(points):
        tangent=Vector(points[min(i+1,len(points)-1)])-Vector(points[max(i-1,0)])
        tangent.normalize()
        normal=tangent.cross(Vector((0,1,0))).normalized()
        if normal.length<.1:normal=Vector((1,0,0))
        second=tangent.cross(normal).normalized()
        for j in range(segments):
            a=math.tau*j/segments
            vertices.append(tuple(Vector(p)+radius*(math.cos(a)*normal+math.sin(a)*second)))
    faces=[]
    for i in range(len(points)-1):
        for j in range(segments):
            a=i*segments+j;b=i*segments+(j+1)%segments
            faces.append((a,b,b+segments,a+segments))
    faces += [tuple(reversed(range(segments))),tuple((len(points)-1)*segments+j for j in range(segments))]
    return mesh_object(name,vertices,faces,CHARCOAL)
mount_parts.append(hose('flamethrower_fuel_hose',[(.079,.285,.369),(.137,.285,.369),(.165,.245,.373),(.165,.150,.390),(.157,.070,.426),(.151,.025,.449),(.147,0,.462)]))
mount_parts.append(lathe('flamethrower_hose_coupling',[(.018,.129),(.018,.151)],DARK_STEEL,8,'X',(0,0,.462)))

# Keep local geometry editable while exposing one yaw and one pitch transform.
for o in base_parts:
    o.parent=root
for o in mount_parts:
    for v in o.data.vertices:v.co.z -= .312
    o.parent=yaw
for o in head_parts:
    for v in o.data.vertices:v.co.z -= .462
    o.parent=pitch
bpy.context.view_layer.update()
socket=empty('MUZZLE_01',(0,.295,0),pitch)
empty('socket_base',(0,0,0),root)
empty('socket_ui_top',(0,0,.620),root)
empty('socket_effect_fuel',(0,-.285,.057),yaw)
pitch['elevation_degrees']=[-10,30]

def combine_group(name, parts, parent):
    """One render primitive per motion group; vertex groups retain part names."""
    bpy.context.view_layer.update()
    vertices=[]; faces=[]; uvs=[]; smooth=[]; ranges=[]
    for part in parts:
        offset=len(vertices)
        transform=parent.matrix_world.inverted() @ part.matrix_world
        vertices.extend(tuple(transform @ v.co) for v in part.data.vertices)
        for poly in part.data.polygons:
            faces.append(tuple(offset+i for i in poly.vertices))
            smooth.append(poly.use_smooth)
            uvs.extend(tuple(part.data.uv_layers.active.data[li].uv) for li in poly.loop_indices)
        ranges.append((part.name,offset,len(part.data.vertices)))
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata(vertices,[],faces);mesh.materials.append(palette)
    uv=mesh.uv_layers.new(name='UVMap')
    for i,co in enumerate(uvs):uv.data[i].uv=co
    for poly,is_smooth in zip(mesh.polygons,smooth):poly.use_smooth=is_smooth
    mesh.set_sharp_from_angle(angle=math.radians(45))
    obj=bpy.data.objects.new(name,mesh);model.objects.link(obj);obj.parent=parent
    for part_name,start,count in ranges:
        obj.vertex_groups.new(name=part_name).add(list(range(start,start+count)),1,'REPLACE')
    for part in parts:
        old_mesh=part.data;bpy.data.objects.remove(part,do_unlink=True)
        if old_mesh.users==0:bpy.data.meshes.remove(old_mesh)
    return obj

base_mesh=combine_group('base_mesh',base_parts,root)
yaw_mesh=combine_group('yaw_mesh',mount_parts,yaw)
pitch_mesh=combine_group('pitch_mesh',head_parts,pitch)

# A real closed 8-vertex box for the game's collision loader, fixed to root.
verts=[(x,y,z) for z in (0,.57) for y in (-.375,.375) for x in (-.375,.375)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
col_data=bpy.data.meshes.new('COL_BOX_BODY');col_data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',col_data);collision.objects.link(col)
col.parent=root;col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('collision_diagnostic');diagnostic.diffuse_color=(1,.1,.7,1)
diagnostic.use_nodes=True
node=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
node.inputs['Base Color'].default_value=(1,.1,.7,1)
node.inputs['Metallic'].default_value=0;node.inputs['Roughness'].default_value=1
col_data.materials.append(diagnostic)

all_model=list(model.objects)+list(collision.objects)
# Final dimensions are baked once into geometry and pivot translations.
for o in all_model:
    o.location *= 2
    if o.type=='MESH':o.data.transform(Matrix.Scale(2,4))
bpy.context.view_layer.update()

def export(path):
    bpy.ops.object.select_all(action='DESELECT')
    for o in all_model:o.select_set(True)
    bpy.context.view_layer.objects.active=base_mesh
    bpy.ops.export_scene.gltf(filepath=str(path),use_selection=True,
        use_active_scene=True,use_visible=False,use_renderable=False,
        export_yup=True,export_texcoords=True,export_normals=True,
        export_materials='EXPORT',export_animations=False,export_extras=True)

export(ROOT/'assets/models/buildings/flamethrower.glb')
# The reference export uses the same contract, with every length halved.
for o in all_model:
    o.location *= .5
    if o.type=='MESH':o.data.transform(Matrix.Scale(.5,4))
bpy.context.view_layer.update()
export(ART/'weapon-flamethrower.glb')
for o in all_model:
    o.location *= 2
    if o.type=='MESH':o.data.transform(Matrix.Scale(2,4))
bpy.context.view_layer.update()

# Neutral preview lighting; studio objects never enter either GLB.
scene.render.engine='BLENDER_EEVEE'
scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard'
scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Flamethrower studio world')
scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('flamethrower_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new(data.name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.56))
data=bpy.data.cameras.new('flamethrower_preview_camera')
cam=bpy.data.objects.new(data.name,data);studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
scene.render.film_transparent=False
views=[('beauty',(-2.5,4,2.5),(0,.03,.54),1.80),('front',(0,4,.60),(0,0,.60),1.66),('side',(-4,0,.60),(0,0,.60),1.66),('back',(0,-4,.60),(0,0,.60),1.66),('top',(0,.002,4),(0,0,0),1.56)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);cam.data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'))
    bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);cam.data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'flamethrower_preview.png')
bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.region_3d.view_perspective='CAMERA'
        area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT')
for o in model.objects:o.select_set(True)
bpy.context.view_layer.objects.active=base_mesh
bpy.data.libraries.write(str(ART/'flamethrower.blend'),{scene},path_remap='RELATIVE',fake_user=True)
# Convert the isolated library into a regular project with this scene active.
# Saving the live main file would also include the user's unrelated scenes.
finalize = """
import bpy
scene = bpy.data.scenes['Flamethrower']
bpy.context.window.scene = scene
for other in list(bpy.data.scenes):
    if other != scene:
        bpy.data.scenes.remove(other)
for area in bpy.context.screen.areas:
    if area.type == 'VIEW_3D':
        area.spaces.active.region_3d.view_perspective = 'CAMERA'
        area.spaces.active.shading.type = 'MATERIAL'
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath, check_existing=False)
"""
subprocess.run([bpy.app.binary_path, '--background', str(ART/'flamethrower.blend'),
                '--python-expr', finalize], check=True)
meshes=[o for o in model.objects if o.type=='MESH']
report={'visible_mesh_count':len(meshes),'visible_triangles':sum(len(p.vertices)-2 for o in meshes for p in o.data.polygons),
    'base_diameter_m':1.2,'height_m':1.14,'reference_scale':.5,'texture':'Original 512x512 colormap.png',
    'rig':['asset_root','yaw_pivot','pitch_pivot','MUZZLE_01','socket_base','socket_ui_top','socket_effect_fuel'],
    'collision':'COL_BOX_BODY: full yaw envelope, 1.5 x 1.14 x 1.5 glTF X/Y/Z metres; 12 triangles',
    'reference_glb':'art/models/weapon-flamethrower.glb','runtime_glb':'assets/models/buildings/flamethrower.glb'}
(VIEWS/'model_report.json').write_text(json.dumps(report,indent=2))
print('FLAMETHROWER_REPORT',json.dumps(report))

# Reference comparisons are preview-only, outside the editable project/GLBs.
comparison=bpy.data.scenes.new('Flamethrower_comparison')
bpy.context.window.scene=comparison
comparison.world=scene.world
comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform
comparison.view_settings.exposure=scene.view_settings.exposure
preview_groups=[]
for i,label in enumerate(('BALLISTA','CANNON','CATAPULT','TURRET','FLAMETHROWER')):
    dx=(i-2)*1.64
    preview_objects=[]
    if label=='FLAMETHROWER':
        sources=[o for o in model.objects if o.type=='MESH']
        for src in sources:
            o=bpy.data.objects.new('comparison_'+src.name,src.data)
            comparison.collection.objects.link(o);o.matrix_world=src.matrix_world.copy()
            o.location.x+=dx;preview_objects.append(o)
    else:
        before=set(comparison.objects)
        bpy.ops.import_scene.gltf(filepath=str(ROOT/'art/references/kenney_tower_defense'/('weapon-'+label.lower()+'.glb')))
        imported=set(comparison.objects)-before
        bpy.context.view_layer.update()
        transforms={o:o.matrix_world.copy() for o in imported if o.type=='MESH'}
        for o,matrix in transforms.items():
            o.parent=None
            o.matrix_world=Matrix.Translation((dx,0,0)) @ Matrix.Rotation(math.pi,4,'Z') @ Matrix.Scale(2,4) @ matrix
            preview_objects.append(o)
    text_data=bpy.data.curves.new('comparison_label_'+label,'FONT')
    text_data.body=label;text_data.align_x='CENTER';text_data.size=.116
    label_obj=bpy.data.objects.new(text_data.name,text_data)
    comparison.collection.objects.link(label_obj);label_obj.location=(dx,.84,-.06)
    label_obj.rotation_euler=(Vector((0,10,6))-label_obj.location).to_track_quat('Z','Y').to_euler()
    preview_groups.append((dx,preview_objects,label_obj))
for src in studio.objects:
    if src.type=='LIGHT':
        o=bpy.data.objects.new('comparison_'+src.name,src.data.copy())
        comparison.collection.objects.link(o);o.matrix_world=src.matrix_world.copy()
        o.data.size*=1.6;o.data.energy*=1.5
cam_data=bpy.data.cameras.new('comparison_camera')
cam=bpy.data.objects.new(cam_data.name,cam_data);comparison.collection.objects.link(cam)
comparison.camera=cam;cam.location=(0,10,6);aim(cam,(0,0,.4))
cam_data.type='ORTHO';cam_data.ortho_scale=8.6
comparison.render.resolution_x=2500;comparison.render.resolution_y=900
comparison.render.resolution_percentage=100
comparison.render.image_settings.file_format='PNG'
comparison.render.filepath=str(ART/'flamethrower_comparison.png')
bpy.ops.render.render(write_still=True)
for i,(dx,objects,label) in enumerate(preview_groups):
    visible=i in (1,4)
    for o in objects:o.hide_render=not visible
    label.hide_render=not visible
    if visible:
        new_x=-.8 if i==1 else .8
        for o in objects:o.location.x+=new_x-dx
        label.location.x=new_x
cam_data.type='PERSP';cam_data.lens=50
cam.location=(-2.4,5.5,3.0);aim(cam,(0,0,.48))
for i in (1,4):
    label=preview_groups[i][2]
    label.rotation_euler=(cam.location-label.location).to_track_quat('Z','Y').to_euler()
comparison.render.resolution_x=1600;comparison.render.resolution_y=1000
comparison.render.filepath=str(VIEWS/'with_cannon.png')
bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene

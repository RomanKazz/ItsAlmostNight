"""Create a Kenney-style resource recycler with open hopper and two functional roller axes.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_resource_recycler_model.py
Final metre units, +Y forward in Blender, input/output sockets and rotating rollers, no weapon muzzle; existing scenes preserved.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Matrix, Vector

import subprocess
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models';VIEWS=ART/'resource_recycler_views';VIEWS.mkdir(parents=True,exist_ok=True)
assert not bpy.data.scenes.get('RESOURCE_RECYCLER'), 'Use a fresh scene or save the existing manual model first'
scene=bpy.data.scenes.new('RESOURCE_RECYCLER');scene['generated_model']='resource_recycler'
bpy.context.window.scene=scene
model=bpy.data.collections.new('RESOURCE_RECYCLER_MODEL');scene.collection.children.link(model)
collision=bpy.data.collections.new('RESOURCE_RECYCLER_COLLISION');scene.collection.children.link(collision)
studio=bpy.data.collections.new('RESOURCE_RECYCLER_STUDIO');scene.collection.children.link(studio)
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
image=bpy.data.images.load(str(ROOT/'assets/models/buildings/Textures/colormap.png'),check_existing=False)
image.name='resource_recycler_colormap';image.pack()
palette=bpy.data.materials.new('resource_recycler_palette');palette.use_nodes=True
shader=next(n for n in palette.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
tex=palette.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
palette.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
energy=palette.copy();energy.name='resource_recycler_energy'
node=next(n for n in energy.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
energy_tex=next(n for n in energy.node_tree.nodes if n.type=='TEX_IMAGE')
energy.node_tree.links.new(energy_tex.outputs['Color'],node.inputs['Emission Color'])
node.inputs['Emission Strength'].default_value=.35
WOOD=(.09375,.275,.475);ORANGE=(.84375,.275,.475);LIGHT_WOOD=(.34375,.275,.475)
STEEL=(.84375,.025,.225);DARK_STEEL=(.59375,.025,.225)
CHARCOAL=(.46875,.025,.225);GREEN=(.53125,.295,.455)

def mesh(name,verts,faces,swatch,material=palette,smooth=()):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);model.objects.link(obj);data.materials.append(material)
    uv=data.uv_layers.new(name='UVMap')
    lo=min(v.co.z for v in data.vertices);hi=max(v.co.z for v in data.vertices)
    for p in data.polygons:
        p.use_smooth=p.index in smooth
        for li in p.loop_indices:
            z=data.vertices[data.loops[li].vertex_index].co.z
            uv.data[li].uv=(swatch[0],swatch[1]+(z-lo)/max(hi-lo,1e-6)*(swatch[2]-swatch[1]))
    data.set_sharp_from_angle(angle=math.radians(45))
    return obj
def lathe(name,profile,swatch,segments=12,axis='Z',center=(0,0,0),smooth=True,phase=0,material=palette,closed=False):
    verts=[];faces=[];smooth_faces=[]
    for r,z in profile:
        for i in range(segments):
            a=phase+math.tau*i/segments;co=Vector((r*math.cos(a),r*math.sin(a),z))
            if axis=='Y':co=Vector((co.x,co.z,-co.y))
            elif axis=='X':co=Vector((co.z,co.y,-co.x))
            verts.append(tuple(co+Vector(center)))
    links=len(profile) if closed else len(profile)-1
    for ring in range(links):
        next_ring=(ring+1)%len(profile)
        for i in range(segments):
            a=ring*segments+i;b=ring*segments+(i+1)%segments
            c=next_ring*segments+(i+1)%segments;d=next_ring*segments+i
            if smooth and abs(profile[next_ring][1]-profile[ring][1])>1e-6:smooth_faces.append(len(faces))
            faces.append((a,b,c,d))
    if not closed:
        faces.extend((tuple(reversed(range(segments))),tuple((len(profile)-1)*segments+i for i in range(segments))))
    return mesh(name,verts,faces,swatch,material,smooth_faces)
def prism(name,outline,xlo,xhi,swatch):
    area=sum(outline[i][0]*outline[(i+1)%len(outline)][1]-outline[(i+1)%len(outline)][0]*outline[i][1] for i in range(len(outline)))
    if area<0:outline=list(reversed(outline))
    n=len(outline);verts=[(x,y,z) for x in (xlo,xhi) for y,z in outline]
    faces=[tuple(reversed(range(n))),tuple(n+i for i in range(n))]
    faces.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
    return mesh(name,verts,faces,swatch)
def empty(name,loc,parent=None):
    o=bpy.data.objects.new(name,None);model.objects.link(o);o.location=loc;o.parent=parent
    o.empty_display_type='PLAIN_AXES';o.empty_display_size=.04
    return o

# Beveled rectangular shells retain the broad flat faces of the supplied concept.
def rectangle(hw,hh,b):
    b=min(b,hw*.75,hh*.75)
    return [(-hw+b,-hh),(hw-b,-hh),(hw,-hh+b),(hw,hh-b),
            (hw-b,hh),(-hw+b,hh),(-hw,hh-b),(-hw,-hh+b)]
def box_profile(name,rings,swatch,zcenter=0,close_profile=False):
    vertices=[]
    for hw,hh,b,y in rings:
        vertices.extend((x,y,zcenter+z) for x,z in rectangle(hw,hh,b))
    faces=[]
    for ring in range(len(rings) if close_profile else len(rings)-1):
        nxt=(ring+1)%len(rings)
        for i in range(8):
            faces.append((ring*8+i,nxt*8+i,nxt*8+(i+1)%8,ring*8+(i+1)%8))
    if not close_profile:
        faces.extend((tuple(range(8)),tuple(reversed(range((len(rings)-1)*8,len(rings)*8)))))
    return mesh(name,vertices,faces,swatch)
def plate(name,outline,ylo,yhi,swatch,material=palette):
    area=sum(outline[i][0]*outline[(i+1)%len(outline)][1]-outline[(i+1)%len(outline)][0]*outline[i][1] for i in range(len(outline)))
    if area<0:outline=list(reversed(outline))
    n=len(outline);vertices=[(x,y,z) for y in (ylo,yhi) for x,z in outline]
    faces=[tuple(range(n)),tuple(reversed(range(n,2*n)))]
    faces.extend((i,i+n,(i+1)%n+n,(i+1)%n) for i in range(n))
    return mesh(name,vertices,faces,swatch,material)
# Closed hollow rectangular shells; the profile loops around the wall thickness.
def square_shell(name,rings,swatch):
    verts=[]
    for hw,hd,b,z in rings:verts.extend((x,y,z) for x,y in rectangle(hw,hd,b))
    faces=[]
    for i in range(len(rings)):
        j=(i+1)%len(rings)
        for k in range(8):faces.append((i*8+k,i*8+(k+1)%8,j*8+(k+1)%8,j*8+k))
    return mesh(name,verts,faces,swatch)

def corner_guard(name,sx,sy):
    verts=[]
    for w,d,b,z in [(.346,.296,.040,.802),(.607,.527,.055,1.173),(.607,.527,.055,1.300),(.590,.510,.050,1.324)]:
        outer=[(w,d-.155),(w,d-b),(w-b,d),(w-.155,d)]
        contour=outer+[(x-.030,y-.030) for x,y in reversed(outer)]
        verts.extend((sx*x,sy*y,z) for x,y in contour)
    faces=[tuple(reversed(range(8))),tuple(range(24,32))]
    for ring in range(3):
        for k in range(8):faces.append((ring*8+k,ring*8+(k+1)%8,(ring+1)*8+(k+1)%8,(ring+1)*8+k))
    # The corner strip is concave; ensure the entire closed shell is outward.
    area=sum(verts[i][0]*verts[(i+1)%8][1]-verts[(i+1)%8][0]*verts[i][1] for i in range(8))
    if area<0:faces=[tuple(reversed(f)) for f in faces]
    return mesh(name,verts,faces,ORANGE)

root=empty('asset_root',(0,0,0));root['asset']='resource_recycler'
root['forward_axis']='Blender +Y / glTF -Z';root['palette']='Original Kenney colormap'
root['variant']='Open feed hopper, twin counter-rotating crushing rollers, discharge chute and green ready light'
body=[];accents=[]
body.append(box_profile('recycler_base',[(.362,.085,.035,-.320),(.390,.085,.035,-.292),(.390,.085,.035,.292),(.362,.085,.035,.320)],DARK_STEEL,.100))
body.append(square_shell('recycler_housing',[(.355,.300,.040,.177),(.400,.340,.060,.225),(.400,.340,.060,.655),(.355,.300,.045,.745),(.290,.235,.030,.745),(.290,.235,.030,.177)],ORANGE))
# Broad corner feet, each physically resting at z=0.
for i in range(4):
    angle=math.pi/4+i*math.pi/2
    leg=prism('recycler_foot_%02d'%i,[(-.102,0),(.102,0),(.102,.110),(.075,.344),(-.075,.344),(-.102,.110)],.310,.470,STEEL)
    for v in leg.data.vertices:v.co.x+=(.344-v.co.z)*.42
    leg.rotation_euler.z=angle;body.append(leg)
    toe=prism('recycler_toe_%02d'%i,[(-.105,0),(.105,0),(.105,.055),(-.105,.055)],.440,.500,ORANGE)
    for v in toe.data.vertices:v.co.x+=(.344-v.co.z)*.42
    toe.rotation_euler.z=angle;body.append(toe)
    bolt=lathe('recycler_bolt_%02d'%i,[(.042,.525),(.042,.580)],DARK_STEEL,8,'X',(0,0,.185),False)
    bolt.rotation_euler.z=angle;body.append(bolt)
# Steel bands around the front/rear housing corners leave the central orange panel broad.
for xsign in (-1,1):
    band=box_profile('recycler_front_band_'+('left' if xsign<0 else 'right'),[(.034,.231,.010,.322),(.034,.231,.010,.358)],DARK_STEEL,.478)
    for v in band.data.vertices:v.co.x+=xsign*.290
    body.append(band)
body.append(square_shell('recycler_throat_seat',[(.340,.288,.035,.714),(.365,.310,.035,.737),(.365,.310,.035,.789),(.340,.288,.035,.807),(.283,.233,.025,.807),(.283,.233,.025,.714)],STEEL))
body.append(square_shell('recycler_hopper',[(.345,.295,.040,.747),(.345,.295,.040,.850),(.580,.500,.055,1.180),(.580,.500,.055,1.279),(.510,.430,.050,1.279),(.510,.430,.050,1.193),(.285,.235,.032,.860),(.285,.235,.032,.747)],DARK_STEEL))
body.append(square_shell('recycler_hopper_rim',[(.581,.501,.055,1.179),(.590,.510,.055,1.196),(.590,.510,.055,1.288),(.575,.495,.050,1.306),(.509,.429,.048,1.306),(.509,.429,.048,1.279),(.519,.439,.048,1.179)],STEEL))
for i,(sx,sy) in enumerate(((1,1),(-1,1),(-1,-1),(1,-1))):body.append(corner_guard('recycler_hopper_corner_%02d'%i,sx,sy))
# Bearing blocks support both axles at the narrow throat.
for sx in (-1,1):
    panel=prism('recycler_bearing_panel_'+('left' if sx<0 else 'right'),[(-.237,.577),(.237,.577),(.237,.826),(.187,.884),(-.187,.884),(-.237,.826)],.334,.430,ORANGE)
    if sx<0:
        for v in panel.data.vertices:v.co.x=-v.co.x
        for p in panel.data.polygons:p.flip()
    body.append(panel)
    for i,y in enumerate((.112,-.112),1):
        profile=[(.064,.421),(.078,.438),(.078,.457),(.064,.468)]
        if sx<0:profile=[(rr,-xx) for rr,xx in reversed(profile)]
        body.append(lathe('recycler_bearing_%s_%02d'%('left' if sx<0 else 'right',i),profile,DARK_STEEL,12,'X',(0,y,.795)))
# A real front outlet frame and a shallow tray, open at its lower end.
body.append(box_profile('recycler_outlet_frame',[(.163,.112,.016,.332),(.176,.112,.016,.351),(.176,.112,.016,.375)],DARK_STEEL,.369))
body.append(box_profile('recycler_outlet_dark',[(.130,.081,.012,.374),(.130,.081,.012,.380)],CHARCOAL,.369))
tray=prism('recycler_output_tray',[(.368,.354),(.750,.172),(.750,.220),(.368,.402)],-.140,.140,STEEL);body.append(tray)
for sx in (-1,1):
    rail=prism('recycler_chute_rail_'+('left' if sx<0 else 'right'),[(.354,.350),(.770,.153),(.770,.251),(.354,.448)],.140,.187,ORANGE)
    if sx<0:
        for v in rail.data.vertices:v.co.x=-v.co.x
        for p in rail.data.polygons:p.flip()
    body.append(rail)
body.append(box_profile('recycler_indicator_frame',[(.119,.058,.010,.338),(.133,.058,.010,.354),(.133,.058,.010,.385),(.118,.047,.008,.398)],DARK_STEEL,.580))
accents.append(box_profile('recycler_indicator',[(.088,.028,.006,.396),(.088,.028,.006,.405)],GREEN,.580))
roller_parts=[]
for i,y in enumerate((.112,-.112),1):
    parts=[]
    parts.append(lathe('recycler_roller_%02d'%i,[(.071,-.267),(.086,-.245),(.086,.245),(.071,.267)],DARK_STEEL,12,'X',(0,y,.795),False))
    parts.append(lathe('recycler_axle_%02d'%i,[(.035,-.478),(.035,.478)],DARK_STEEL,8,'X',(0,y,.795),False))
    for k in range(6):
        a=k*math.tau/6
        tooth=box_profile('recycler_tooth_%02d_%02d'%(i,k),[(.020,.019,.004,-.234),(.020,.019,.004,.234)],STEEL)
        for v in tooth.data.vertices:
            x=v.co.y;u=v.co.x;z=v.co.z+.083
            v.co=(x,y+u*math.cos(a)-z*math.sin(a),.795+u*math.sin(a)+z*math.cos(a))
        parts.append(tooth)
    # Faceted side drive cap follows the same axle and clears its bearing face.
    side=-1 if i==1 else 1
    profile=[(.106,.474),(.127,.490),(.127,.521),(.105,.536)]
    if side<0:profile=[(rr,-xx) for rr,xx in reversed(profile)]
    parts.append(lathe('recycler_drive_%02d'%i,profile,DARK_STEEL,12,'X',(0,y,.795),False))
    roller_parts.append(parts)

def combine(name,parts,parent,material):
    bpy.context.view_layer.update();verts=[];faces=[];uvs=[];flags=[];ranges=[]
    for o in parts:
        offset=len(verts);matrix=parent.matrix_world.inverted()@o.matrix_world
        verts.extend(tuple(matrix@v.co) for v in o.data.vertices)
        for p in o.data.polygons:
            faces.append(tuple(offset+i for i in p.vertices));flags.append(p.use_smooth)
            uvs.extend(tuple(o.data.uv_layers.active.data[li].uv) for li in p.loop_indices)
        ranges.append((o.name,offset,len(o.data.vertices)))
    o=mesh(name,verts,faces,WOOD,material);o.parent=parent
    for i,co in enumerate(uvs):o.data.uv_layers.active.data[i].uv=co
    for p,flag in zip(o.data.polygons,flags):p.use_smooth=flag
    for label,start,count in ranges:o.vertex_groups.new(name=label).add(list(range(start,start+count)),1,'REPLACE')
    for part in parts:
        data=part.data;bpy.data.objects.remove(part,do_unlink=True)
        if data.users==0:bpy.data.meshes.remove(data)
    return o
base=combine('base_mesh',body,root,palette)
service=combine('indicator_mesh',accents,root,energy)
rollers=[]
for i,(y,parts) in enumerate(zip((.112,-.112),roller_parts),1):
    pivot=empty('roller_%02d_pivot'%i,(0,y,.795),root)
    bpy.context.view_layer.update()
    rollers.append(combine('roller_%02d_mesh'%i,parts,pivot,palette))
empty('socket_base',(0,0,0),root)
empty('socket_ui_top',(0,0,1.420),root)
empty('socket_input',(0,0,1.310),root)
empty('socket_output',(0,.775,.235),root)
empty('socket_status',(0,.410,.580),root)
verts=[(x,y,z) for z in (0,1.350) for y in (-.560,.790) for x in (-.610,.610)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
data=bpy.data.meshes.new('COL_BOX_BODY');data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',data);collision.objects.link(col);col.parent=root
col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('resource_recycler_collision');diagnostic.use_nodes=True
shader=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(1,.1,.7,1);shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
data.materials.append(diagnostic)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in list(model.objects)+list(collision.objects):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/resource_recycler.glb'),use_selection=True,
    use_active_scene=True,use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,
    export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Resource recycler_world');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('resource_recycler_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new('resource_recycler_'+name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.65))
data=bpy.data.cameras.new('resource_recycler_camera');cam=bpy.data.objects.new('resource_recycler_camera',data);studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
views=[('beauty',(-2.8,4,2.9),(0,.06,.63),2.05),('front',(0,4,.66),(0,0,.66),1.85),('side',(-4,0,.66),(0,0,.66),1.85),('back',(0,-4,.66),(0,0,.66),1.85),('top',(0,.001,4),(0,0,0),1.55)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'resource_recycler_preview.png');bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.data.libraries.write(str(ART/'resource_recycler.blend'),{scene},path_remap='ABSOLUTE',fake_user=True)
finalize = """import bpy
s=bpy.data.scenes['RESOURCE_RECYCLER']
bpy.context.window.scene=s
for other in list(bpy.data.scenes):
    if other!=s:bpy.data.scenes.remove(other)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.region_3d.view_perspective='CAMERA'
        area.spaces.active.shading.type='MATERIAL'
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath,check_existing=False)
"""
subprocess.run([bpy.app.binary_path,'--background',str(ART/'resource_recycler.blend'),
                '--python-exit-code','1','--python-expr',finalize],check=True)

comparison=bpy.data.scenes.new('Resource recycler_comparison');bpy.context.window.scene=comparison
comparison.world=scene.world;comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform;comparison.view_settings.exposure=scene.view_settings.exposure
for i,label in enumerate(('CANNON','REPAIR_STATION','RESOURCE_RECYCLER')):
    dx=(i-1)*1.65
    if label=='RESOURCE_RECYCLER':
        for src in (base,service,*rollers):
            o=bpy.data.objects.new('comparison_'+src.name,src.data);comparison.collection.objects.link(o)
            o.matrix_world=src.matrix_world.copy();o.location.x+=dx
    else:
        before=set(comparison.objects)
        path=ROOT/('art/references/kenney_tower_defense/weapon-cannon.glb' if label=='CANNON' else 'assets/models/buildings/repair_station.glb')
        bpy.ops.import_scene.gltf(filepath=str(path));bpy.context.view_layer.update()
        imported=set(comparison.objects)-before;transforms={o:o.matrix_world.copy() for o in imported if o.type=='MESH'}
        for o,matrix in transforms.items():
            o.parent=None
            extra=Matrix.Rotation(math.pi,4,'Z')@Matrix.Scale(2,4) if label=='CANNON' else Matrix.Identity(4)
            o.matrix_world=Matrix.Translation((dx,0,0))@extra@matrix
            if o.name.startswith('COL_'):o.hide_render=True
    text=bpy.data.curves.new('label_'+label,'FONT');text.body=label;text.align_x='CENTER';text.size=.10
    o=bpy.data.objects.new(text.name,text);comparison.collection.objects.link(o);o.location=(dx,.83,-.05)
    o.rotation_euler=(Vector((2,8.5,4.2))-o.location).to_track_quat('Z','Y').to_euler()
for src in studio.objects:
    if src.type=='LIGHT':
        o=bpy.data.objects.new('comparison_'+src.name,src.data.copy());comparison.collection.objects.link(o)
        o.matrix_world=src.matrix_world.copy();o.data.size*=1.5;o.data.energy*=1.3
data=bpy.data.cameras.new('comparison_camera');cam=bpy.data.objects.new(data.name,data);comparison.collection.objects.link(cam);comparison.camera=cam
cam.location=(2,8.5,4.2);aim(cam,(0,0,.55));data.lens=50
comparison.render.resolution_x=1800;comparison.render.resolution_y=950;comparison.render.resolution_percentage=100
comparison.render.image_settings.file_format='PNG';comparison.render.filepath=str(ART/'resource_recycler_comparison.png');bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene
print('RESOURCE_RECYCLER_CREATED',json.dumps({'visible_triangles':sum(len(p.vertices)-2 for o in (base,service,*rollers) for p in o.data.polygons),'visible_meshes':4,'width':1.214,'height':1.324,'roller_axis':'Blender X / GLB X'}))


"""Create a compact wood harvester and reusable resource-family pedestal from the generated concept.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_wood_harvester_model.py
Final metre units, +Y forward in Blender, saw pivot and resource sockets, no weapon muzzle; existing scenes preserved.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Matrix, Vector

import subprocess
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models';VIEWS=ART/'wood_harvester_views';VIEWS.mkdir(parents=True,exist_ok=True)
assert not bpy.data.scenes.get('WOOD_HARVESTER'), 'Use a fresh scene or save the existing manual model first'
scene=bpy.data.scenes.new('WOOD_HARVESTER');scene['generated_model']='wood_harvester'
bpy.context.window.scene=scene
model=bpy.data.collections.new('WOOD_HARVESTER_MODEL');scene.collection.children.link(model)
collision=bpy.data.collections.new('WOOD_HARVESTER_COLLISION');scene.collection.children.link(collision)
studio=bpy.data.collections.new('WOOD_HARVESTER_STUDIO');scene.collection.children.link(studio)
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
image=bpy.data.images.load(str(ROOT/'assets/models/buildings/Textures/colormap.png'),check_existing=False)
image.name='wood_harvester_colormap';image.pack()
palette=bpy.data.materials.new('wood_harvester_palette');palette.use_nodes=True
shader=next(n for n in palette.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
tex=palette.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
palette.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
WOOD=(.09375,.275,.475);ORANGE=(.84375,.275,.475);LIGHT_WOOD=(.34375,.275,.475)
STEEL=(.84375,.025,.225);DARK_STEEL=(.59375,.025,.225)
CHARCOAL=(.46875,.025,.225);BARK=(.15625,.275,.475);BARK_DARK=(.21875,.275,.475)

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
import sys
sys.path.insert(0,str(ROOT/'tools'))
from harvester_family_base import build_base,FAMILY_ID,MOUNT_HEIGHT
root=empty('asset_root',(0,0,0));root['asset']='wood_harvester'
root['forward_axis']='Blender +Y / glTF -Z';root['family']=FAMILY_ID
root['variant']='Compact circular saw and log cradle on the shared octagonal resource pedestal'
saw=empty('saw_pivot',(-.100,-.060,.630),root)
base_parts=build_base(lathe,ORANGE,STEEL,DARK_STEEL)
module_parts=[];saw_parts=[];resource_parts=[]
# Short, fully supported worktable and open side guides.
for i,(lo,hi) in enumerate(((.072,.105),(.295,.325))):
    leg=box_profile('wood_table_leg_%02d'%i,[(.080,.043,.008,lo),(.080,.043,.008,hi)],DARK_STEEL,.247)
    for v in leg.data.vertices:v.co.x-=.100
    module_parts.append(leg)
table=box_profile('wood_feed_table',[(.140,.013,.007,.052),(.150,.013,.007,.066),(.150,.013,.007,.400),(.140,.013,.007,.414)],STEEL,.297)
for v in table.data.vertices:v.co.x-=.100
module_parts.append(table)
for side,x in (('left',-.245),('right',.045)):
    rail=box_profile('wood_guide_'+side,[(.012,.041,.006,.115),(.012,.041,.006,.290)],DARK_STEEL,.338)
    for v in rail.data.vertices:v.co.x+=x
    module_parts.append(rail)
# Saw axle is supported by a broad A-frame; the motor is on its rear side.
frame=plate('wood_saw_frame',[(-.218,.207),(.018,.207),(.018,.273),(-.056,.657),(-.075,.676),(-.125,.676),(-.144,.657),(-.218,.273)],-.151,-.112,DARK_STEEL)
module_parts.append(frame)
module_parts.append(lathe('wood_motor',[(.064,-.324),(.083,-.306),(.083,-.205),(.069,-.182)],ORANGE,8,'Y',(-.100,0,.630),False))
module_parts.append(lathe('wood_motor_bearing',[(.048,-.185),(.057,-.169),(.057,-.107),(.045,-.091)],DARK_STEEL,8,'Y',(-.100,0,.630),False))
module_parts.append(lathe('wood_motor_end',[(.058,-.332),(.058,-.320)],DARK_STEEL,8,'Y',(-.100,0,.630),False))
for i,z in enumerate((.607,.630,.653)):
    vent=box_profile('wood_motor_vent_%02d'%i,[(.033,.005,.003,-.335),(.033,.005,.003,-.331)],CHARCOAL,z)
    for v in vent.data.vertices:v.co.x-=.100
    module_parts.append(vent)
# The orange guard surrounds only the lower/right arc, with real radial clearance.
guard_outline=[]
for radius,angles in ((.280,[1.30+2.19*i/12 for i in range(13)]),(.252,[1.30+2.19*i/12 for i in reversed(range(13))])):
    guard_outline.extend((-.100+radius*math.sin(a),.630+radius*math.cos(a)) for a in angles)
module_parts.append(plate('wood_saw_guard',guard_outline,-.100,-.012,ORANGE))
seat=box_profile('wood_guard_seat',[(.047,.082,.008,-.120),(.047,.082,.008,-.039)],ORANGE,.286)
for v in seat.data.vertices:v.co.x-=.100
module_parts.append(seat)
# Compact two-log magazine in an integrated side cradle.
tray=box_profile('wood_stock_tray',[(.092,.015,.006,-.245),(.092,.015,.006,.229)],STEEL,.275)
for v in tray.data.vertices:v.co.x+=.250
module_parts.append(tray)
for i,y in enumerate((-.125,.125)):
    support=box_profile('wood_stock_foot_%02d'%i,[(.065,.029,.006,y-.019),(.065,.029,.006,y+.019)],DARK_STEEL,.233)
    for v in support.data.vertices:v.co.x+=.250
    module_parts.append(support)
for i,(lo,hi) in enumerate(((-.199,-.174),(.130,.155))):
    module_parts.append(plate('wood_stock_cradle_%02d'%i,[(.155,.260),(.345,.260),(.345,.554),(.322,.554),(.322,.289),(.178,.289),(.178,.554),(.155,.554)],lo,hi,ORANGE))
# Log meshes keep clear of the saw in their neutral presentation pose.
def log(name,x,z,radius,ylo,yhi,swatch,growth_rings=True):
    parts=[lathe(name+'_bark',[(radius*.88,ylo),(radius,ylo+.014),(radius,yhi-.014),(radius*.92,yhi)],swatch,8,'Y',(x,0,z),False)]
    parts.append(lathe(name+'_front_end',[(radius*.90,yhi-.001),(radius*.90,yhi+.003)],LIGHT_WOOD,8,'Y',(x,0,z),False))
    parts.append(lathe(name+'_rear_end',[(radius*.86,ylo-.002),(radius*.86,ylo+.001)],LIGHT_WOOD,8,'Y',(x,0,z),False))
    for i,ratio in enumerate((.59,.34) if growth_rings else (.50,)):
        rr=radius*ratio
        parts.append(lathe(name+'_growth_%02d'%i,[(rr-.0017,yhi+.0035),(rr+.0017,yhi+.0035),(rr+.0017,yhi+.0045),(rr-.0017,yhi+.0045)],WOOD,8,'Y',(x,0,z),False,closed=True))
    return parts
resource_parts.extend(log('wood_work_log',-.100,.403,.093,.075,.405,BARK))
resource_parts.extend(log('wood_stock_lower',.250,.356,.066,-.233,.213,BARK_DARK,False))
resource_parts.extend(log('wood_stock_upper',.250,.490,.066,-.233,.213,BARK,False))
# Sixteen chunky teeth; the closed blade and hub are a single rotating group.
outline=[]
for i in range(16):
    for fraction,radius in ((0,.214),(.23,.243),(.53,.243),(.77,.214)):
        a=(i+fraction)*math.tau/16
        outline.append((-.100+radius*math.sin(a),.630+radius*math.cos(a)))
saw_parts.append(plate('wood_saw_blade',outline,-.068,-.052,STEEL))
saw_parts.append(lathe('wood_saw_inner_disc',[(.145,-.054),(.145,-.047)],DARK_STEEL,16,'Y',(-.100,0,.630),False))
saw_parts.append(lathe('wood_saw_axle',[(.023,-.216),(.023,-.026)],DARK_STEEL,8,'Y',(-.100,0,.630),False))
saw_parts.append(lathe('wood_saw_hub',[(.042,-.049),(.057,-.035),(.057,-.008),(.043,.005)],ORANGE,8,'Y',(-.100,0,.630),False))
saw_parts.append(lathe('wood_saw_bolt',[(.028,.003),(.028,.021)],DARK_STEEL,8,'Y',(-.100,0,.630),False))

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
base=combine('base_mesh',base_parts,root,palette)
module=combine('module_mesh',module_parts,root,palette)
resource=combine('resource_mesh',resource_parts,root,palette)
saw_mesh=combine('saw_mesh',saw_parts,saw,palette)
empty('socket_base',(0,0,0),root)
empty('socket_resource_mount',(0,0,MOUNT_HEIGHT),root)
empty('socket_ui_top',(0,0,.980),root)
empty('socket_output',(-.100,.425,.403),root)
empty('socket_effect_sawdust',(-.100,.060,.410),root)
verts=[(x,y,z) for z in (0,.900) for y in (-.480,.480) for x in (-.480,.480)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
data=bpy.data.meshes.new('COL_BOX_BODY');data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',data);collision.objects.link(col);col.parent=root
col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('wood_harvester_collision');diagnostic.use_nodes=True
shader=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(1,.1,.7,1);shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
data.materials.append(diagnostic)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in list(model.objects)+list(collision.objects):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/wood_harvester.glb'),use_selection=True,
    use_active_scene=True,use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,
    export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Wood harvester_world');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('wood_harvester_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new('wood_harvester_'+name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.65))
data=bpy.data.cameras.new('wood_harvester_camera');cam=bpy.data.objects.new('wood_harvester_camera',data);studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
views=[('beauty',(-2.8,4,2.7),(0,0,.425),1.42),('front',(0,4,.44),(0,0,.44),1.20),('side',(-4,0,.44),(0,0,.44),1.20),('back',(0,-4,.44),(0,0,.44),1.20),('top',(0,.001,4),(0,0,0),1.20)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'wood_harvester_preview.png');bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.data.libraries.write(str(ART/'wood_harvester.blend'),{scene},path_remap='ABSOLUTE',fake_user=True)
finalize = """import bpy
s=bpy.data.scenes['WOOD_HARVESTER']
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
subprocess.run([bpy.app.binary_path,'--background',str(ART/'wood_harvester.blend'),
                '--python-exit-code','1','--python-expr',finalize],check=True)

comparison=bpy.data.scenes.new('Wood harvester_comparison');bpy.context.window.scene=comparison
comparison.world=scene.world;comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform;comparison.view_settings.exposure=scene.view_settings.exposure
for i,label in enumerate(('CRYSTAL_MINE','QUARRY','WOOD_HARVESTER')):
    dx=(i-1)*1.65
    if label=='WOOD_HARVESTER':
        for src in (base,module,resource,saw_mesh):
            o=bpy.data.objects.new('comparison_'+src.name,src.data);comparison.collection.objects.link(o)
            o.matrix_world=src.matrix_world.copy();o.location.x+=dx
    else:
        before=set(comparison.objects)
        path=ROOT/('assets/models/buildings/mine.glb' if label=='CRYSTAL_MINE' else 'assets/models/buildings/quarry.glb')
        bpy.ops.import_scene.gltf(filepath=str(path));bpy.context.view_layer.update()
        imported=set(comparison.objects)-before;transforms={o:o.matrix_world.copy() for o in imported if o.type=='MESH'}
        ordered=sorted(transforms,key=lambda o:o.name)
        # Runtime draws the L1 replacement housing and original shards, not all baked levels.
        if label=='CRYSTAL_MINE':
            chosen=[o for o in ordered if o.name.startswith('upgrade_crystal_mine_l1')]
            shard=next(o for o in ordered if o.name.startswith('detail-crystal-large'))
            # The original mine contains body and crystals as two primitives; isolate its shard material.
            data=shard.data.copy();copy=bpy.data.objects.new('comparison_crystal_shards',data);comparison.collection.objects.link(copy)
            copy.matrix_world=transforms[shard];chosen.append(copy)
            import bmesh
            bm=bmesh.new();bm.from_mesh(data)
            # Purple crystal faces use U=.46875; the old green housing uses U=.59375.
            uv=bm.loops.layers.uv.active
            discard=[f for f in bm.faces if all(loop[uv].uv.x>.5 for loop in f.loops)]
            bmesh.ops.delete(bm,geom=discard,context='FACES');bm.to_mesh(data);bm.free()
            transforms[copy]=copy.matrix_world.copy()
        else:chosen=[o for o in ordered if o.name.startswith('upgrade_quarry_l1')]
        for o in list(transforms):
            if o not in chosen:o.hide_render=True
        factor=1.05 if label=='CRYSTAL_MINE' else .65
        for o in chosen:
            matrix=transforms[o];o.parent=None
            o.matrix_world=Matrix.Translation((dx,0,0))@Matrix.Scale(factor,4)@matrix
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
comparison.render.image_settings.file_format='PNG';comparison.render.filepath=str(ART/'wood_harvester_comparison.png');bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene
print('WOOD_HARVESTER_CREATED',json.dumps({'visible_triangles':sum(len(p.vertices)-2 for o in (base,module,resource,saw_mesh) for p in o.data.polygons),'visible_meshes':4,'width':.918,'height':.873,'family':FAMILY_ID}))


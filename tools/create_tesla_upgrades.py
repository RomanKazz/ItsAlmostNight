"""Author Tesla levels 1–8 in an isolated scene; callable through Blender MCP.
Base asset stays byte-for-byte intact. L2–L8 are complete single-mesh overlays.
Call build_references(), build_levels(), then render_level()/render_comparison().
"""
from pathlib import Path
import hashlib, json, math, struct
import bpy
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/tesla_levels'
ART.mkdir(parents=True,exist_ok=True)
BASE=ROOT/'assets/models/buildings/tesla_tower.glb'
PALETTE=ROOT/'assets/models/buildings/Textures/colormap.png'
GOLD=(.65625,.285,.465); RED=(.96875,.285,.465)
ORANGE=(.84375,.275,.475); DARK=(.59375,.035,.215); CYAN=(.03125,.105,.185)

def new_scene(name):
    # Never clear the user's open scene or overwrite an existing authored scene.
    scene=bpy.data.scenes.new(name);bpy.context.window.scene=scene
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    return scene

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
def studio(scene):
    collection=bpy.data.collections.new(scene.name+'_STUDIO');scene.collection.children.link(collection)
    scene.render.engine='BLENDER_EEVEE'
    scene.render.resolution_x=600;scene.render.resolution_y=600;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'
    scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.35
    world=bpy.data.worlds.new(scene.name+'_world');world.use_nodes=True;scene.world=world
    bg=world.node_tree.nodes.get('Background');bg.inputs['Color'].default_value=(.13,.17,.20,1);bg.inputs['Strength'].default_value=.65
    for name,pos,power,size in [('key',(2,4,6),440,6),('fill',(-4,2,4),300,6),('rim',(0,-4,5),400,4)]:
        data=bpy.data.lights.new(scene.name+'_'+name,'AREA');data.energy=power;data.size=size
        obj=bpy.data.objects.new(data.name,data);collection.objects.link(obj);obj.location=pos;aim(obj,(0,0,.65))
    data=bpy.data.cameras.new(scene.name+'_camera');cam=bpy.data.objects.new(data.name,data)
    collection.objects.link(cam);scene.camera=cam;data.type='ORTHO';data.ortho_scale=2.0
    cam.location=(2.5,4,2.7);aim(cam,(0,0,.72))
    return collection

def import_visible(path,keep=lambda o:True,offset=0,scale=1):
    scene=bpy.context.scene;before=set(scene.objects)
    bpy.ops.import_scene.gltf(filepath=str(path));bpy.context.view_layer.update()
    imported=set(scene.objects)-before;visible=[]
    for obj in imported:
        if obj.type=='MESH' and not obj.name.startswith('COL_') and keep(obj):
            world=obj.matrix_world.copy();obj.parent=None
            obj.matrix_world=Matrix.Translation((offset,0,0))@Matrix.Scale(scale,4)@world
            obj.hide_render=False;visible.append(obj)
    for obj in imported-set(visible):bpy.data.objects.remove(obj,do_unlink=True)
    return visible

def build_references():
    scene=new_scene('TESLA_LEVEL_STYLE_REFERENCES');studio(scene)
    import_visible(ROOT/'assets/models/buildings/core_levels.glb',lambda o:o.name.startswith(('core_pedestal_l8','core_crystal','core_satellite')),-2.5,1.76)
    import_visible(ROOT/'assets/models/buildings/cannon.glb',lambda o:'upgrade_' not in o.name or '_l8_' in o.name,-.8)
    import_visible(ROOT/'assets/models/buildings/flamethrower.glb',offset=.9)
    import_visible(ROOT/'assets/models/buildings/flamethrower_upgrades.glb',lambda o:'_l8_' in o.name,.9)
    import_visible(BASE,offset=2.5)
    scene.camera.location=(2,10,5);aim(scene.camera,(0,0,.70));scene.camera.data.ortho_scale=7.2
    scene.render.resolution_x=1600;scene.render.resolution_y=650
    scene.render.filepath=str(ART/'style_references.png');bpy.ops.render.render(write_still=True)
    return scene

class Geometry:
    def __init__(self):self.verts=[];self.faces=[];self.swatches=[];self.smooth=[];self.groups={}
    def add(self,name,verts,faces,swatch,smooth=()):
        start=len(self.verts);self.verts.extend(verts)
        self.groups[name]=(start,len(verts))
        self.faces.extend(tuple(start+i for i in f) for f in faces)
        self.swatches.extend([swatch]*len(faces));self.smooth.extend(i in smooth for i in range(len(faces)))
    def ring(self,name,lo,hi,inner,outer,swatch,center=(0,0),sides=12):
        profile=[(inner,lo),(outer,lo),(outer,hi),(inner,hi)]
        verts=[(center[0]+r*math.cos(math.tau*i/sides),center[1]+r*math.sin(math.tau*i/sides),z) for r,z in profile for i in range(sides)]
        faces=[];smooth=[]
        for b in range(4):
            for i in range(sides):
                j=(i+1)%sides;k=(b+1)%4
                faces.append((b*sides+i,b*sides+j,k*sides+j,k*sides+i))
                if b in (1,3):smooth.append(len(faces)-1)
        self.add(name,verts,faces,swatch,smooth)
    def lathe(self,name,profile,swatch,center=(0,0),sides=12):
        verts=[(center[0]+r*math.cos(math.tau*i/sides),center[1]+r*math.sin(math.tau*i/sides),z) for r,z in profile for i in range(sides)]
        faces=[];smooth=[]
        for b in range(len(profile)-1):
            for i in range(sides):
                j=(i+1)%sides;faces.append((b*sides+i,b*sides+j,(b+1)*sides+j,(b+1)*sides+i))
                if abs(profile[b][1]-profile[b+1][1])>1e-6:smooth.append(len(faces)-1)
        faces.extend([tuple(reversed(range(sides))),tuple((len(profile)-1)*sides+i for i in range(sides))])
        self.add(name,verts,faces,swatch,smooth)
    def prism(self,name,outline,half_width,angle,swatch):
        # Radial cross section (radius,height), extruded tangentially.
        verts=[]
        for tangential in (-half_width,half_width):
            for radial,z in outline:
                verts.append((radial*math.cos(angle)-tangential*math.sin(angle),radial*math.sin(angle)+tangential*math.cos(angle),z))
        n=len(outline);faces=[tuple(range(n)),tuple(reversed(range(n,2*n)))]
        faces.extend((i,i+n,(i+1)%n+n,(i+1)%n) for i in range(n))
        self.add(name,verts,faces,swatch)
    def object(self,name,collection,material):
        mesh=bpy.data.meshes.new(name);mesh.from_pydata(self.verts,[],self.faces);mesh.update()
        # Recalculate winding for capped profiles and radial prisms alike.
        import bmesh
        bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
        obj=bpy.data.objects.new(name,mesh);collection.objects.link(obj);mesh.materials.append(material)
        uv=mesh.uv_layers.new(name='UVMap')
        for poly,swatch,smooth in zip(mesh.polygons,self.swatches,self.smooth):
            poly.use_smooth=smooth
            heights=[mesh.vertices[i].co.z for i in poly.vertices];lo=min(heights);hi=max(heights)
            for li in poly.loop_indices:
                z=mesh.vertices[mesh.loops[li].vertex_index].co.z
                t=(z-lo)/max(1e-6,hi-lo) if hi-lo>1e-6 else .65
                uv.data[li].uv=(swatch[0],swatch[1]+(swatch[2]-swatch[1])*t)
        for name,(start,count) in self.groups.items():obj.vertex_groups.new(name=name).add(list(range(start,start+count)),1,'REPLACE')
        return obj

def build_levels():
    global scene,levels,source,model_collection
    original_hash=hashlib.sha256(BASE.read_bytes()).hexdigest()
    scene=new_scene('TESLA_LEVELS');studio(scene)
    model_collection=bpy.data.collections.new('TESLA_UPGRADE_MODEL');scene.collection.children.link(model_collection)
    source_collection=bpy.data.collections.new('TESLA_BASE_REFERENCE');scene.collection.children.link(source_collection)
    before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(BASE));bpy.context.view_layer.update()
    source=list(set(scene.objects)-before)
    for obj in source:
        for c in list(obj.users_collection):c.objects.unlink(obj)
        source_collection.objects.link(obj)
        if obj.name.startswith('COL_'):obj.hide_render=True
    image=bpy.data.images.load(str(PALETTE),check_existing=False);image.pack()
    mat=bpy.data.materials.new('tesla_upgrade_colormap');mat.use_nodes=True
    bsdf=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    bsdf.inputs['Metallic'].default_value=0;bsdf.inputs['Roughness'].default_value=1
    tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
    mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
    levels={}
    for level in range(2,9):
        g=Geometry()
        # Broad straps touch the original foot/collars, with no coplanar skins.
        g.ring('gold_foot_belt',.155,.218,.587,.623,GOLD)
        g.ring('terminal_gold_lip',1.035,1.068,.244,.276,GOLD)
        if level>=3:
            for i in range(4):
                a=i*math.pi/2
                g.prism('shoulder_gold_'+str(i),[(.395,.29),(.505,.29),(.435,.57),(.385,.65),(.32,.65),(.35,.57)],.10,a,GOLD)
                g.prism('shoulder_orange_'+str(i),[(.485,.325),(.505,.325),(.437,.555),(.400,.605),(.382,.605),(.417,.548)],.071,a,ORANGE)
                # Complete replacement bolt head covers the old bolt/armor intersection.
                for name,profile,swatch in [
                    ('washer',[(.073,.467),(.073,.512)],GOLD),
                    ('bolt',[(.051,.505),(.051,.546),(.042,.555)],DARK)]:
                    start=len(g.verts)
                    g.lathe('shoulder_'+name+'_'+str(i),profile,swatch,sides=8)
                    for v in range(start,len(g.verts)):
                        x,y,r=g.verts[v]
                        g.verts[v]=(r*math.cos(a)-x*math.sin(a),r*math.sin(a)+x*math.cos(a),.395+y)

        if level>=4:
            g.ring('lower_coil_band',.577,.604,.251,.280,GOLD)
            for i,z in enumerate((.696,.804,.912)):
                g.ring('coil_gold_rim_'+str(i),z-.009,z+.013,.180,.204,GOLD)
        if level>=5:
            # Four capacitor banks fit between the structural braces.
            for i in range(4):
                a=math.pi/4+i*math.pi/2;c=(.405*math.cos(a),.405*math.sin(a))
                top=.66 if level<6 else .86
                g.lathe('capacitor_foot_'+str(i),[(.112,.275),(.121,.297),(.121,.335),(.105,.348)],GOLD,c)
                g.lathe('capacitor_cell_'+str(i),[(.090,.33),(.103,.35),(.103,top-.035),(.088,top)],RED,c)
                g.lathe('capacitor_cap_'+str(i),[(.108,top-.035),(.115,top-.016),(.115,top+.020),(.088,top+.040)],GOLD,c)
                g.ring('capacitor_band_'+str(i),.415,.452,.098,.113,GOLD,c)
                if level>=6:
                    g.ring('capacitor_second_band_'+str(i),.708,.742,.098,.114,GOLD,c)
                    g.lathe('capacitor_terminal_'+str(i),[(.048,top+.036),(.048,top+.075),(.027,top+.093)],CYAN,c,8)
        if level>=7:
            # Four chamfered crown prongs remain below/away from the orb surface.
            g.ring('crown_base',1.006,1.052,.254,.310,GOLD)
            for i in range(4):
                a=math.pi/4+i*math.pi/2
                g.prism('crown_prong_'+str(i),[(.263,1.025),(.333,1.025),(.355,1.175),(.327,1.235),(.297,1.175)],.038,a,GOLD)
                g.prism('crown_inlay_'+str(i),[(.335,1.080),(.350,1.172),(.328,1.212),(.318,1.170)],.039,a,ORANGE)
        if level>=8:
            g.ring('master_base_belt',.093,.135,.586,.638,GOLD)
            g.ring('master_red_belt',.221,.258,.589,.626,RED)
            g.ring('emitter_gold_seat',1.084,1.117,.118,.163,GOLD)
            for i in range(4):
                a=i*math.pi/2
                g.prism('master_shoulder_'+str(i),[(.335,.573),(.442,.573),(.458,.624),(.407,.686),(.328,.686)],.115,a,GOLD)
                # Broad raised rank chevron, outside the shoulder face.
                g.prism('master_insignia_'+str(i),[(.453,.60),(.462,.62),(.412,.675),(.400,.658)],.073,a,RED)
        obj=g.object(f'tesla_upgrade_l{level}',model_collection,mat)
        obj['level']=level;obj['mode']='complete additive overlay; draw only this level'
        obj.hide_render=True;levels[level]=obj
    bpy.ops.object.select_all(action='DESELECT')
    for obj in levels.values():obj.select_set(True)
    bpy.context.view_layer.objects.active=levels[8]
    output=ROOT/'assets/models/buildings/tesla_upgrades.glb'
    bpy.ops.export_scene.gltf(filepath=str(output),use_selection=True,use_active_scene=True,use_visible=False,use_renderable=False,
        export_yup=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)
    raw=output.read_bytes();d=json.loads(raw[20:20+struct.unpack_from('<I',raw,12)[0]])
    rows=[]
    for index,m in enumerate(d['meshes']):
        p=m['primitives'][0];a=d['accessors'][p['attributes']['POSITION']]
        rows.append({'level':int(m['name'].rsplit('l',1)[1]),'mesh':index,'bounds_glb':[a['min'],a['max']],
                     'triangles':d['accessors'][p['indices']]['count']//3})
    rows.sort(key=lambda r:r['level'])
    manifest={'mode':'L1 unchanged base; L2-L8 base + one complete static overlay','base_sha256':original_hash,
        'palette_sha256':hashlib.sha256(PALETTE.read_bytes()).hexdigest(),'footprint_m':[2,2],
        'levels':rows,'additional_draws':1,'overlay_materials':len(d['materials']),
        'base_bounds_glb':[[-.6,0,-.6],[.6,1.44,.6]],'collision':'unchanged base COL_BOX_BODY',
        'energy_center_glb':[0,1.26,0],'muzzle_glb':[0,1.26,-.2],
        'motion':'static C4 symmetric; root transforms identity; no added pivots',
        'integration':'renderer selects current level; same draw path for preview, shadows and outline',
        'license':'Kenney colormap CC0; original project upgrade geometry','native_playtest':False}
    (ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    header='#pragma once\n#include <algorithm>\n#include <array>\n#include <raylib.h>\nnamespace ian::tesla_upgrade_model {\n'
    header+='// Generated by tools/create_tesla_upgrades.py; one complete overlay per level.\n'
    header+='inline constexpr std::array<int,7> Meshes{'+','.join(str(r['mesh']) for r in rows)+'};\n'
    header+='inline int meshIndex(int level) { return level<=1 ? -1 : Meshes[static_cast<std::size_t>(std::clamp(level,2,8)-2)]; }\n'
    header+='}\n'
    (ROOT/'src/graphics/TeslaUpgradeModel.hpp').write_text(header)
    levels[8].hide_render=False
    for obj in levels.values():obj.hide_set(obj!=levels[8])
    bpy.ops.object.select_all(action='DESELECT');levels[8].select_set(True)
    for area in bpy.context.screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
    # Write only our scene; finalize its active scene in a separate Blender process.
    bpy.data.libraries.write(str(ART/'tesla_levels.blend'),{scene},fake_user=True)
    assert hashlib.sha256(BASE.read_bytes()).hexdigest()==original_hash
    print('TESLA_UPGRADES',json.dumps(manifest))

def render_level(level,view='beauty'):
    bpy.context.window.scene=scene
    for lv,obj in levels.items():obj.hide_render=lv!=level
    cam=scene.camera
    views={'beauty':((2.5,4,2.7),(0,0,.72),2.05),'front':((0,5,.75),(0,0,.75),1.8),
           'side':((5,0,.75),(0,0,.75),1.8),'back':((0,-5,.75),(0,0,.75),1.8),'top':((0,.001,5),(0,0,0),1.6)}
    pos,target,scale=views[view];cam.location=pos;aim(cam,target);cam.data.ortho_scale=scale
    scene.render.filepath=str(ART/f'level_{level}_{view}.png');bpy.ops.render.render(write_still=True)

if __name__=='__main__':
    build_references();build_levels()
    for level in range(1,9):render_level(level)
    for view in ('front','side','back','top'):render_level(8,view)

"""Create two low-poly shrine GLBs in an isolated Blender scene via MCP.

Uses the original packed Kenney colormap. Does not overwrite other scenes.
"""
from pathlib import Path
import json, math, hashlib
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/shrines';ART.mkdir(parents=True,exist_ok=True)
OUT=ROOT/'assets/models/world_landmarks';OUT.mkdir(parents=True,exist_ok=True)
old=bpy.data.scenes.get('IAN_SHRINES')
if old:
    for o in list(old.objects):bpy.data.objects.remove(o,do_unlink=True)
    bpy.data.scenes.remove(old)
scene=bpy.data.scenes.new('IAN_SHRINES');bpy.context.window.scene=scene
palette=ROOT/'assets/models/buildings/Textures/colormap.png'
image=bpy.data.images.load(str(palette),check_existing=False);image.pack()
mat=bpy.data.materials.new('Shrine_Kenney_Colormap');mat.use_nodes=True
node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
node.inputs['Roughness'].default_value=1
tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
mat.node_tree.links.new(tex.outputs['Color'],node.inputs['Base Color'])
STEEL=(.84375,.125);DARK=(.59375,.125);GOLD=(.65625,.375)
PURPLE=(.46875,.625);CYAN=(.03125,.625);RED=(.96875,.375)

class Geo:
    def __init__(self):self.v=[];self.f=[];self.uv=[];self.s=[];self.groups={}
    def face(self,indices,uv,smooth=False):self.f.append(indices);self.uv.append(uv);self.s.append(smooth)
    def lathe(self,name,profile,uv,center=(0,0,0),sides=12):
        start=len(self.v)
        for r,z in profile:
            for i in range(sides):
                a=math.tau*i/sides;self.v.append((center[0]+r*math.cos(a),center[1]+r*math.sin(a),center[2]+z))
        for j in range(len(profile)):
            k=(j+1)%len(profile)
            for i in range(sides):
                n=(i+1)%sides
                self.face((start+j*sides+i,start+j*sides+n,start+k*sides+n,start+k*sides+i),uv,abs(profile[j][1]-profile[k][1])>1e-6)
        self.groups[name]=list(range(start,len(self.v)))
    def emboss(self,name,outline,z,uv,hole=0):
        start=len(self.v);n=len(outline)
        for y in (-.11,.11):self.v.extend((x,y,z+h) for x,h in outline)
        if not hole:
            self.face(tuple(start+i for i in range(n)),uv)
            self.face(tuple(start+n+i for i in reversed(range(n))),uv)
        else:
            for y in (-.11,.11):
                for x,h in outline:
                    a=math.atan2(h,x);self.v.append((hole*math.cos(a),y,z+hole*math.sin(a)))
            for i in range(n):
                k=(i+1)%n
                self.face((start+i,start+k,start+2*n+k,start+2*n+i),uv)
                self.face((start+n+k,start+n+i,start+3*n+i,start+3*n+k),uv)
                self.face((start+2*n+i,start+2*n+k,start+3*n+k,start+3*n+i),uv)
        for i in range(n):
            k=(i+1)%n;self.face((start+i,start+n+i,start+n+k,start+k),uv)
        self.groups[name]=list(range(start,len(self.v)))
    def gem(self,z,uv):
        start=len(self.v)
        self.v.extend([(0,-.18,z),(0,.18,z),(.18,0,z),(0,0,z+.18),(-.18,0,z),(0,0,z-.18)])
        for i,k in [(2,3),(3,4),(4,5),(5,2)]:
            self.face((start,start+i,start+k),uv);self.face((start+1,start+k,start+i),uv)
    def object(self,name):
        mesh=bpy.data.meshes.new(name);mesh.from_pydata(self.v,[],self.f);mesh.update();mesh.materials.append(mat)
        layer=mesh.uv_layers.new(name='UVMap')
        for p,uv,smooth in zip(mesh.polygons,self.uv,self.s):
            p.use_smooth=smooth
            for loop in p.loop_indices:layer.data[loop].uv=uv
        for edge in mesh.edges:
            a,b=(mesh.vertices[i].co for i in edge.vertices)
            if abs(a.z-b.z)<1e-6:edge.use_edge_sharp=True
        obj=bpy.data.objects.new(name,mesh);scene.collection.objects.link(obj)
        for group,verts in self.groups.items():obj.vertex_groups.new(name=group).add(verts,1,'REPLACE')
        return obj

roots=[];manifest={}
for kind in ('player','building'):
    root=bpy.data.objects.new(kind+'_shrine_root',None);scene.collection.objects.link(root);roots.append(root)
    g=Geo();accent=PURPLE if kind=='player' else CYAN
    g.lathe('stone_steps',[(.01,0),(.92,0),(.92,.12),(.84,.18),(.84,.30),(.70,.37),(.01,.37)],STEEL)
    g.lathe('gold_step_edge',[(.79,.18),(.86,.18),(.86,.24),(.79,.24)],GOLD)
    g.lathe('colored_altar',[(.01,.36),(.62,.36),(.62,.49),(.43,.58),(.43,1.25),(.56,1.30),(.56,1.40),(.01,1.40)],accent)
    g.lathe('gold_cup',[(.01,1.39),(.64,1.39),(.64,1.50),(.54,1.57),(.01,1.57)],GOLD)
    for i in range(4):
        a=math.pi*.25+i*math.pi*.5;c=(.63*math.cos(a),.63*math.sin(a),0)
        g.lathe('pillar_'+str(i),[(.01,.34),(.11,.34),(.14,.42),(.11,.53),(.11,1.43),(.15,1.48),(.15,1.59),(.01,1.59)],STEEL,c,8)
        g.lathe('pillar_cap_'+str(i),[(.01,1.60),(.15,1.60),(.15,1.70),(.01,1.70)],GOLD,c,8)
    body=g.object(kind+'_shrine_body');body.parent=root
    g=Geo()
    if kind=='player':
        outline=[]
        # Heart outline is reversed below to face outwards on both sides.
        for i in range(32):
            t=math.tau*i/32
            outline.append((.035*16*math.sin(t)**3,.035*(13*math.cos(t)-5*math.cos(2*t)-2*math.cos(3*t)-math.cos(4*t))))
        g.emboss('heart_blessing',list(reversed(outline)),2.20,RED)
    else:
        outline=[]
        for i in range(32):
            a=math.tau*i/32;r=.53 if i%4 in (1,2) else .44
            outline.append((r*math.cos(a),r*math.sin(a)))
        # Counterclockwise XZ outline: front faces point toward -Y.
        g.emboss('gear_blessing',outline,2.20,GOLD,.23)
        g.gem(2.20,CYAN)
    glyph=g.object(kind+'_shrine_blessing');glyph.parent=root
    bpy.ops.object.select_all(action='DESELECT');root.select_set(True);body.select_set(True);glyph.select_set(True);bpy.context.view_layer.objects.active=body
    bpy.ops.export_scene.gltf(filepath=str(OUT/(kind+'_shrine.glb')),export_format='GLB',use_selection=True,use_active_scene=True,export_yup=True)
    coords=[v.co for o in (body,glyph) for v in o.data.vertices]
    manifest[kind]={'glb':'assets/models/world_landmarks/'+kind+'_shrine.glb','authoring_bounds_min':[min(v[i] for v in coords) for i in (0,2,1)],'authoring_bounds_max':[max(v[i] for v in coords) for i in (0,2,1)],'runtime_height':2.8,'primitives':2,'material_count':1,'collision':'Simulation: static centered 2x2m blocking box, height 2.8m; hover from actual rendered meshes','pivot_glb':[0,0,0],'transforms':'neutral root, vertices baked; Blender Z-up / GLB Y-up','palette_sha256':hashlib.sha256(palette.read_bytes()).hexdigest()}
    root.location.x=-1.3 if kind=='player' else 1.3

world=bpy.data.worlds.new('Shrine_Studio');world.use_nodes=True;scene.world=world
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.045,.06,.08,1);bg.inputs[1].default_value=.8
for name,loc,power in [('key',(1,-4,6),650),('fill',(-4,-1,4),350),('rim',(0,4,5),450)]:
    data=bpy.data.lights.new('Shrine_'+name,'AREA');data.energy=power;data.size=5
    obj=bpy.data.objects.new('Shrine_'+name,data);scene.collection.objects.link(obj);obj.location=loc;obj.rotation_euler=(-obj.location).to_track_quat('-Z','Y').to_euler()
cam=bpy.data.objects.new('Shrine_Camera',bpy.data.cameras.new('Shrine_Camera'));scene.collection.objects.link(cam);scene.camera=cam
cam.location=(3,-7,4);cam.rotation_euler=(Vector((0,0,1.1))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=5.6
scene.render.resolution_x=1200;scene.render.resolution_y=800;scene.render.resolution_percentage=100
scene.view_settings.view_transform='Standard';scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ART/'preview.png')
try:scene.render.engine='BLENDER_EEVEE'
except TypeError:pass
bpy.data.libraries.write(str(ART/'shrines.blend'),{scene},fake_user=True)
(ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Exported two shrines: original packed palette, 2 meshes each, neutral roots.')

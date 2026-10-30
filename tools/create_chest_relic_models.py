"""Hand-authored low-poly relic family. Execute via Blender MCP; never clears user scenes."""
from pathlib import Path
import bpy, math, json
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/models/items/relics'; OUT.mkdir(parents=True,exist_ok=True)
ART=ROOT/'art/models/chest_relics'; ART.mkdir(parents=True,exist_ok=True)
scene=bpy.data.scenes.new('Chest Relics Authoring')
bpy.context.window.scene=scene
COLORS=[(147,78,32),(216,145,61),(253,201,107),(54,66,80),(113,147,164),(199,224,226),
        (255,181,29),(255,226,111),(74,166,62),(142,221,67),(36,138,204),(84,220,237),
        (174,66,70),(245,94,74),(124,75,184),(233,186,245)]
atlas=bpy.data.images.new('Relic Palette',width=256,height=128,alpha=False)
pixels=[]
for y in range(128):
 for x in range(256):
  c=COLORS[x//16]; shade=.58+.52*y/127
  pixels.extend([min(1,c0/255*shade) for c0 in c]+[1])
atlas.pixels=pixels;atlas.filepath_raw=str(ART/'palette.png');atlas.file_format='PNG';atlas.save();atlas.pack()
mat=bpy.data.materials.new('Relic Palette Material');mat.use_nodes=True
shader=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED');shader.inputs['Roughness'].default_value=.8
tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=atlas
mat.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
current=None
parts=[]
def finish(o,name,c,bevel=0):
 o.name=name
 for coll in list(o.users_collection):coll.objects.unlink(o)
 current.objects.link(o)
 bpy.context.view_layer.objects.active=o
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True)
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 if bevel:
  m=o.modifiers.new('Soft edge','BEVEL');m.width=bevel;m.segments=1
  bpy.ops.object.modifier_apply(modifier=m.name)
 o.data.materials.clear();o.data.materials.append(mat)
 for old_uv in list(o.data.uv_layers):o.data.uv_layers.remove(old_uv)
 uv=o.data.uv_layers.new(name='Palette UV')
 for face in o.data.polygons:
  light=.58+.28*max(0,face.normal.z)+.14*max(0,-face.normal.y)
  for loop in face.loop_indices:uv.data[loop].uv=((c+.5)/len(COLORS),light)
 o['palette_color']=c;parts.append(o);return o
def box(name,p,size,c,bevel=.025):
 bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.scale=size
 return finish(o,name,c,bevel)
def cyl(name,p,r,depth,c,vertices=12,rot=None,r2=None):
 if r2 is None:
  bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=depth,location=p)
 else:bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=r,radius2=r2,depth=depth,location=p)
 o=bpy.context.object
 if rot:o.rotation_euler=rot
 return finish(o,name,c)
def ball(name,p,size,c):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=6,radius=1,location=p);o=bpy.context.object;o.scale=size
 return finish(o,name,c)
def rod(name,a,b,r,c):
 a,b=Vector(a),Vector(b);o=cyl(name,(a+b)/2,r,(b-a).length,c,8);o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();return o
def torus(name,p,major,minor,c,rot=None):
 bpy.ops.mesh.primitive_torus_add(major_segments=16,minor_segments=6,location=p,major_radius=major,minor_radius=minor)
 o=bpy.context.object
 if rot:o.rotation_euler=rot
 return finish(o,name,c)
def emblem(p,c=7):
 box('Cross vertical',p,(.08,.025,.32),c,.008);box('Cross horizontal',p,(.28,.025,.08),c,.008)
def model(name):
 global current,parts
 current=bpy.data.collections.new(name);scene.collection.children.link(current);parts=[]
models=[]
def export(name):
 bpy.ops.object.select_all(action='DESELECT')
 for o in parts:o.select_set(True)
 bpy.context.view_layer.objects.active=parts[0]
 # Editable named components stay in source; only temporary copy is merged for runtime.
 copies=[]
 for o in parts:
  q=o.copy();q.data=o.data.copy();current.objects.link(q);copies.append(q)
 bpy.ops.object.select_all(action='DESELECT')
 for q in copies:q.select_set(True)
 bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();merged=bpy.context.object;merged.name=name+'_runtime'
 bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 bounds=[merged.matrix_world@Vector(v) for v in merged.bound_box]
 lo=Vector([min(v[i] for v in bounds) for i in range(3)]);hi=Vector([max(v[i] for v in bounds) for i in range(3)])
 center=(lo+hi)/2;factor=1.30/max(hi-lo)
 # world-centered neutral transform makes spinning pickups stable.
 for v in merged.data.vertices:v.co=(merged.matrix_world@v.co-center)*factor
 merged.matrix_world.identity()
 bpy.ops.export_scene.gltf(filepath=str(OUT/(name+'.glb')),export_format='GLB',use_selection=True,use_active_scene=True,export_materials='EXPORT',export_yup=True)
 models.append({'slug':name,'triangles':sum(len(p.vertices)-2 for p in merged.data.polygons),'parts':[o.name for o in parts],'max_extent':1.30})
 bpy.data.objects.remove(merged,do_unlink=True)
 for o in parts:o.hide_render=True;o.hide_set(True)

model('Acorn')
ball('Nut',(0,0,.48),(.36,.30,.45),1);cyl('Cap',(0,0,.77),.4,.22,0,12,r2=.34)
for i in range(12):
 a=i*math.tau/12;ball('Cap scales',(.33*math.cos(a),.27*math.sin(a),.80),(.085,.08,.065),1)
rod('Stem',(0,0,.86),(.07,0,1.08),.065,0);export('acorn')
model('Crystal Pan')
cyl('Bowl',(0,0,.25),.38,.16,3,12,r2=.51);cyl('Bowl inside',(0,0,.34),.43,.018,4)
for x,y,z in [(-.15,-.08,.41),(.1,.02,.45),(.25,.1,.40)]:
 o=cyl('Crystal',(x,y,z),.10,.30,11,5,r2=0);o.rotation_euler[1]=.35
rod('Handle',(.4,.02,.28),(.85,.02,.28),.075,0);export('crystal_pan')
model('Repair Nail')
cyl('Iron shaft',(0,0,.5),.07,.80,4,8);cyl('Nail point',(0,0,.07),.0,.16,5,8,r2=.07);cyl('Heavy nail head',(0,0,.95),.22,.13,5,8)
box('Head stripe',(0,-.15,.96),(.18,.03,.035),6,.005);export('repair_nail')
model('Lumber Boot')
box('Sole',(.13,0,.13),(.82,.42,.17),3);box('Leather toe',(.23,0,.30),(.62,.40,.29),1,.07)
box('Boot ankle',(-.09,0,.61),(.38,.38,.62),0,.04);box('Cuff',(-.09,0,.94),(.47,.44,.13),1)
for z in [.52,.64,.76]:box('Lace',(-.09,-.211,z),(.25,.035,.035),2,.005)
export('lumber_boot')
model('Abacus')
for x in [-.45,.45]:box('Wood frame',(x,0,.57),(.12,.20,1.10),0)
for z in [.07,1.07]:box('Frame end',(0,0,z),(.95,.20,.12),1)
for row,z in enumerate([.27,.49,.71,.93]):
 rod('Counting rod',(-.41,0,z),(.41,0,z),.025,4)
 for j in range(4):ball('Counting bead',(-.29+j*.17+(row%2)*.08,0,z),(.07,.10,.095),6 if row%2 else 11)
export('abacus')
model('Weapon Token')
cyl('Medallion',(0,0,.53),.43,.15,6,12,(math.pi/2,0,0));cyl('Inset',(0,-.085,.53),.34,.025,0,12,(math.pi/2,0,0))
rod('Blade',(-.2,-.12,.31),(.2,-.12,.79),.045,5);rod('Blade 2',(.2,-.12,.31),(-.2,-.12,.79),.045,5)
torus('Loop',(0,0,1.01),.10,.035,6,(math.pi/2,0,0));export('weapon_token')
model('Coiled Spring')
for i in range(150):
 a=i/149*math.tau*4;b=(i+1)/149*math.tau*4
 if i<149:rod('Steel coil',(.25*math.cos(a),.25*math.sin(a),.17+i/149*.76),(.25*math.cos(b),.25*math.sin(b),.17+(i+1)/149*.76),.045,5)
for z in [.10,1.0]:cyl('Spring end',(0,0,z),.32,.09,4,12)
export('idle_spring')
model('Repair Magnet')
for x,c in [(-.27,12),(.27,10)]:
 box('Magnet arm',(x,0,.62),(.23,.27,.65),c,.05);box('Metal pole',(x,0,1.01),(.25,.29,.18),5)
ball('Magnet bend',(0,0,.34),(.38,.15,.26),12);box('Magnet inset',(0,-.16,.4),(.29,.03,.25),3)
emblem((0,-.19,.38));export('repair_magnet')
model('Siren')
cyl('Metal base',(0,0,.15),.40,.20,3);cyl('Brass ring',(0,0,.30),.36,.12,6)
cyl('Red glass',(0,0,.55),.32,.40,13,12,r2=.24);ball('Dome',(0,0,.77),(.24,.24,.14),13)
box('Light reflection',(-.12,-.25,.57),(.06,.025,.24),7,.01);export('siren')
model('Clover')
rod('Stem',(0,0,.0),(.06,0,.52),.045,8)
for x,z in [(-.18,.69),(.18,.69),(-.18,.96),(.18,.96)]:ball('Clover leaf',(x,0,z),(.23,.08,.23),9)
ball('Heart',(0,-.07,.82),(.09,.06,.09),8);export('clover')
model('Mining Hammer')
rod('Hickory handle',(0,0,.09),(0,0,.97),.095,0)
for z in [.18,.28,.38]:cyl('Grip band',(0,0,z),.102,.04,1)
box('Hammer head',(0,0,.97),(.82,.34,.34),4,.05);box('Bright face',(-.39,0,.97),(.09,.36,.36),5)
box('Gold collar',(0,0,.97),(.16,.36,.37),6);export('mining_hammer')
model('Dawn Coin')
cyl('Coin',(0,0,.61),.46,.13,6,16,(math.pi/2,0,0));cyl('Face',(0,-.075,.61),.37,.025,7,16,(math.pi/2,0,0))
ball('Sun',(0,-.11,.61),(.15,.035,.15),6)
for i in range(8):
 a=i*math.tau/8;rod('Sun ray',(.22*math.cos(a),-.11,.61+.22*math.sin(a)),(.30*math.cos(a),-.11,.61+.30*math.sin(a)),.025,6)
export('dawn_coin')
model('Ghost Candle')
cyl('Candle dish',(0,0,.1),.35,.09,6);cyl('Wax',(0,0,.48),.21,.68,15,12)
for x,z in [(-.14,.65),(.10,.72),(.02,.57)]:ball('Wax drip',(x,-.17,z),(.05,.055,.16),15)
rod('Wick',(0,0,.80),(0,0,.91),.025,3);ball('Flame',(0,0,1.02),(.105,.08,.22),7);ball('Flame core',(0,-.06,.98),(.055,.025,.13),13)
export('ghost_candle')
model('Core Fuse')
for z in [.16,.92]:cyl('Brass cap',(0,0,z),.26,.22,6)
cyl('Ceramic fuse',(0,0,.54),.21,.60,5,12)
rod('Wire zig',(-.13,-.225,.26),(.13,-.225,.50),.025,13);rod('Wire zig2',(.13,-.225,.50),(-.13,-.225,.79),.025,13)
box('Bolt label',(0,-.235,.55),(.1,.04,.16),6,.01);export('core_fuse')
model('Crystal Safe')
box('Safe casing',(0,0,.60),(.84,.63,.91),3,.07);box('Safe door',(0,-.35,.60),(.72,.10,.78),4,.04)
for x in [-.25,.25]:
 for z in [.29,.91]:ball('Rivet',(x,-.42,z),(.035,.02,.035),6)
torus('Combination wheel',(0,-.43,.65),.16,.032,6,(math.pi/2,0,0))
for a in [0,math.tau/3,2*math.tau/3]:rod('Wheel spoke',(0,-.43,.65),(.15*math.cos(a),-.43,.65+.15*math.sin(a)),.023,6)
cyl('Crystal emblem',(.23,-.43,.42),.08,.22,11,5,r2=0);export('crystal_safe')
model('Rebuild Blueprint')
box('Blueprint paper',(0,0,.60),(.74,.05,.85),10,.015)
for x in [-.41,.41]:cyl('Rolled end',(x,0,.60),.075,.96,11,10)
for x in [-.13,.13]:box('Tower plan',(x,-.035,.64),(.045,.025,.36),5,.005)
box('Plan top',(0,-.035,.86),(.42,.025,.05),5,.005);box('Plan base',(0,-.035,.43),(.42,.025,.05),5,.005)
rod('Plan barrel',(0,-.04,.85),(.24,-.04,.98),.025,5);export('rebuild_blueprint')
model('Loaded Die')
box('Ivory die',(0,0,.58),(.78,.70,.78),2,.08)
for x,z in [(-.20,.38),(.2,.38),(0,.58),(-.2,.78),(.2,.78)]:ball('Front pip',(x,-.355,z),(.065,.015,.065),3)
for y,z in [(-.17,.40),(.17,.76),(-.17,.76)]:ball('Side pip',(.395,y,z),(.015,.065,.065),3)
export('lucky_die')
model('Upgrade Mirror')
box('Mirror frame',(0,0,.69),(.71,.18,.90),6,.10);box('Glass',(0,-.11,.69),(.56,.035,.73),11,.08)
box('Reflection stripe',(-.10,-.135,.72),(.075,.015,.52),5,.015)
rod('Mirror handle',(0,0,.02),(0,0,.30),.09,0);cyl('Pommel',(0,0,.05),.12,.10,6)
export('upgrade_mirror')
# Unhide parts in authoring scene; arrange models in a tidy gallery.
for index,record in enumerate(models):
 coll=bpy.data.collections[record['slug'].replace('_',' ').title()] if False else scene.collection.children[index]
 for o in coll.objects:
  o.hide_render=False;o.hide_set(False);o.location.x+=(index%6)*1.8;o.location.z+=(2-index//6)*1.65
scene['runtime_models']=json.dumps(models)
bpy.data.libraries.write(str(ART/'chest_relics.blend'),{scene},fake_user=True)
(ART/'manifest.json').write_text(json.dumps({'license':'Original game assets, authored for ItsAlmostNight','palette':'Chunky low-poly tools / existing powerup palette family','models':models},indent=2))
print('Created and exported',len(models),'relic models. Source:',ART/'chest_relics.blend')

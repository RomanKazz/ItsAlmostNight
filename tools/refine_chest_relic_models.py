"""Second authored pass. Each relic has its own scene, editable parts and review export.
Use through Blender MCP: import module, build(slug), review, then gallery().
Stage outputs stay separate until approved by local visual and loader checks.
"""
from pathlib import Path
import bpy, math, json, shutil, time
from mathutils import Vector, Matrix
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/chest_relics/refined';OUT=ART/'runtime'
OUT.mkdir(parents=True,exist_ok=True)
# Warm wood, cool machined metal, saturated jewel accents: existing tool/powerup family.
PALETTE=[(77,42,25),(126,65,30),(181,100,43),(225,146,65),(252,191,100),
 (31,46,61),(62,89,112),(104,140,160),(162,197,206),(226,240,227),
 (148,83,15),(216,139,21),(255,199,57),(255,231,142),
 (157,36,43),(232,66,51),(255,142,81),(20,92,155),(31,159,212),(102,224,235),
 (34,105,42),(65,160,47),(150,216,71),(99,52,130),(166,99,200),(231,181,239),
 (241,215,166),(188,153,105),(102,58,52),(49,38,72),(150,199,229),(242,251,247)]
WOOD=2;LIGHTWOOD=3;DARKWOOD=1;LEATHER=1;EDGE=0
INK=5;DARKSTEEL=6;STEEL=7;SILVER=8;WHITE=9
BRONZE=10;GOLD=11;GOLDLIGHT=12;GLOW=13
DARKRED=14;RED=15;ORANGE=16;BLUE=17;SKY=18;CYAN=19
DARKGREEN=20;GREEN=21;LIME=22;PURPLE=23;VIOLET=24;LILAC=25;IVORY=26;PARCHMENT=27
atlas=bpy.data.images.new('Relics Refined Palette',width=512,height=128,alpha=False)
p=[]
for y in range(128):
 for x in range(512):
  color=PALETTE[x//16]; shade=.63+.43*y/127
  p.extend([min(1,c/255*shade) for c in color]+[1])
atlas.pixels=p;atlas.filepath_raw=str(ART/'palette.png');atlas.file_format='PNG';atlas.save();atlas.pack()
mat=bpy.data.materials.new('Relic Crafted Palette');mat.use_nodes=True
bsdf=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED');bsdf.inputs['Roughness'].default_value=.72
tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=atlas
mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
scene=None;coll=None;parts=[];records={};active_slug=None

def select(objects):
 for o in bpy.context.view_layer.objects:o.select_set(False)
 for o in objects:o.select_set(True)
 if objects:bpy.context.view_layer.objects.active=objects[0]

def shade(o,color,bevel=0,smooth=False,segments=2):
 for c in list(o.users_collection):c.objects.unlink(o)
 coll.objects.link(o);select([o])
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 if bevel:
  m=o.modifiers.new('Crafted bevel','BEVEL');m.width=bevel;m.segments=segments
  bpy.ops.object.modifier_apply(modifier=m.name)
 for f in o.data.polygons:f.use_smooth=smooth
 if smooth and bevel:
  try:
   n=o.modifiers.new('Weighted broad faces','WEIGHTED_NORMAL');n.keep_sharp=True;n.weight=40
   bpy.ops.object.modifier_apply(modifier=n.name)
  except (RuntimeError,TypeError):pass
 for uv in list(o.data.uv_layers):o.data.uv_layers.remove(uv)
 o.data.materials.clear();o.data.materials.append(mat);o['palette_color']=color
 parts.append(o);return o

def box(name,p,size,c,bevel=.025,rotation=None):
 bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.name=name;o.scale=size
 o=shade(o,c,bevel,True)
 if rotation:o.rotation_euler=rotation
 return o

def lathe(name,profile,c,segments=20,ratio=1,center=(0,0,0),smooth=True):
 # Closed surfaces can include a return profile for a hollow bowl/tube.
 v=[(center[0]+r*math.cos(i*math.tau/segments),center[1]+r*ratio*math.sin(i*math.tau/segments),center[2]+z)
    for z,r in profile for i in range(segments)]
 faces=[tuple(reversed(range(segments))),tuple(range((len(profile)-1)*segments,len(profile)*segments))]
 for j in range(len(profile)-1):
  for i in range(segments):
   k=j*segments+i;n=j*segments+(i+1)%segments
   faces.append((k,n,n+segments,k+segments))
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(v,[],faces);mesh.update()
 o=bpy.data.objects.new(name,mesh);coll.objects.link(o);o=shade(o,c,0,smooth)
 # Flat end caps, smooth rotational walls.
 for f in mesh.polygons:
  if abs(f.normal.z)>.995:f.use_smooth=False
 return o

def cyl(name,p,r,depth,c,vertices=16,rot=None,r2=None,bevel=.008):
 if r2 is None:bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=depth,location=p)
 else:bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=r,radius2=r2,depth=depth,location=p)
 o=bpy.context.object;o.name=name;o=shade(o,c,bevel,True)
 if rot:o.rotation_euler=rot
 return o

def ball(name,p,size,c,segments=16,rings=8):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,radius=1,location=p)
 o=bpy.context.object;o.name=name;o.scale=size;return shade(o,c,0,True)

def ico(name,p,size,c,sub=1):
 bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=sub,radius=1,location=p)
 o=bpy.context.object;o.name=name;o.scale=size;return shade(o,c)

def meshpart(name,verts,faces,c,bevel=0,smooth=False):
 m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update()
 o=bpy.data.objects.new(name,m);coll.objects.link(o);return shade(o,c,bevel,smooth)

def prism(name,points,depth,c,y=0,bevel=.01,smooth=True):
 clean=[]
 for point in points:
  if not clean or math.dist(point,clean[-1])>1e-7:clean.append(point)
 if len(clean)>1 and math.dist(clean[0],clean[-1])<1e-7:clean.pop()
 points=clean
 if sum(points[i][0]*points[(i+1)%len(points)][1]-points[(i+1)%len(points)][0]*points[i][1] for i in range(len(points)))<0:points.reverse()
 n=len(points);v=[(x,y-depth/2,z) for x,z in points]+[(x,y+depth/2,z) for x,z in points]
 faces=[tuple(range(n)),tuple(reversed(range(n,n*2)))]+[(i,i+n,(i+1)%n+n,(i+1)%n) for i in range(n)]
 return meshpart(name,v,faces,c,bevel,smooth)

def tube(name,points,r,c,sides=8,closed=False):
 points=[Vector(p) for p in points];verts=[]
 for i,p in enumerate(points):
  tangent=(points[(i+1)%len(points)]-points[(i-1)%len(points)]) if closed else points[min(i+1,len(points)-1)]-points[max(0,i-1)]
  tangent.normalize();axis=Vector((0,0,1)) if abs(tangent.z)<.9 else Vector((0,1,0))
  u=tangent.cross(axis).normalized();v=tangent.cross(u).normalized()
  rr=r[i] if isinstance(r,(list,tuple)) else r
  for j in range(sides):verts.append(tuple(p+rr*(u*math.cos(j*math.tau/sides)+v*math.sin(j*math.tau/sides))))
 faces=[]
 for i in range(len(points) if closed else len(points)-1):
  ni=(i+1)%len(points)
  for j in range(sides):faces.append((i*sides+j,i*sides+(j+1)%sides,ni*sides+(j+1)%sides,ni*sides+j))
 if not closed:faces += [tuple(reversed(range(sides))),tuple(range((len(points)-1)*sides,len(points)*sides))]
 return meshpart(name,verts,faces,c,0,True)

def rod(name,a,b,r,c):return tube(name,[a,b],r,c,8)
def ring(name,p,r,wire,c,rot=None):
 bpy.ops.mesh.primitive_torus_add(major_segments=24,minor_segments=8,location=p,major_radius=r,minor_radius=wire)
 o=bpy.context.object;o.name=name;o=shade(o,c,0,True)
 if rot:o.rotation_euler=rot
 return o

def screw(name,p,size=.022,c=SILVER):
 cyl(name,p,size,size*.5,c,10,(math.pi/2,0,0),bevel=.003)
 box(name+' slot',(p[0],p[1]-size*.28,p[2]),(size*1.05,.005,size*.17),INK,.001,rotation=(0,.3,0))

def curve_points(fn,n=20):return [fn(i/(n-1)) for i in range(n)]
def leaf(name,base,tip,width,c,curve=.07):
 base,tip=Vector(base),Vector(tip);axis=tip-base;side=axis.cross(Vector((0,-1,0))).normalized()
 verts=[]
 # Raised central vein, curved margin, real back thickness.
 for i in range(7):
  t=i/6;mid=base+axis*t;bulge=math.sin(math.pi*t);mid.y-=curve*bulge
  w=width*math.sin(math.pi*t)**.8
  verts.extend([tuple(mid-side*w+Vector((0,.025*bulge,0))),tuple(mid-Vector((0,.025*bulge,0))),tuple(mid+side*w+Vector((0,.025*bulge,0)))])
 faces=[]
 for i in range(6):
  for j in range(2):a=i*3+j;faces.append((a,a+1,a+4,a+3))
 o=meshpart(name,verts,faces,c,0,True)
 select([o]);solid=o.modifiers.new('Leaf thickness','SOLIDIFY');solid.thickness=.025;bpy.ops.object.modifier_apply(modifier=solid.name)
 return o

def bake_palette(o):
 select([o]);bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 m=o.data;m.update()
 for old_uv in list(m.uv_layers):m.uv_layers.remove(old_uv)
 uv=m.uv_layers.new(name='RelicPalette')
 normals=m.corner_normals if hasattr(m,'corner_normals') else None
 color=o['palette_color']
 light=Vector((-.35,-.65,.85)).normalized()
 for face in m.polygons:
  for li in face.loop_indices:
   normal=face.normal if not face.use_smooth else (normals[li].vector if normals is not None else m.vertices[m.loops[li].vertex_index].normal)
   shade=.45+.40*max(0,normal.dot(light))+.06*max(0,normal.z)
   uv.data[li].uv=((color+.5)/32,min(.96,shade))

def build(slug):
 global scene,coll,parts,active_slug
 if slug not in BUILDERS:raise ValueError(slug)
 scene=bpy.data.scenes.new('CRAFTED_'+slug);bpy.context.window.scene=scene
 coll=bpy.data.collections.new(slug.upper()+'_MODEL');scene.collection.children.link(coll);parts=[];active_slug=slug
 BUILDERS[slug]()
 for o in parts:bake_palette(o)
 copies=[]
 for o in parts:q=o.copy();q.data=o.data.copy();coll.objects.link(q);copies.append(q)
 select(copies);bpy.ops.object.join();merged=bpy.context.object;merged.name=slug+'_runtime'
 bpy.context.view_layer.update();bounds=[merged.matrix_world@Vector(v) for v in merged.bound_box]
 lo=Vector([min(v[i] for v in bounds) for i in range(3)]);hi=Vector([max(v[i] for v in bounds) for i in range(3)]);center=(lo+hi)/2;factor=1.3/max(hi-lo)
 for v in merged.data.vertices:v.co=(merged.matrix_world@v.co-center)*factor
 merged.matrix_world=Matrix.Identity(4)
 clean_mesh(merged)
 select([merged]);bpy.ops.export_scene.gltf(filepath=str(OUT/(slug+'.glb')),export_format='GLB',use_selection=True,use_active_scene=True,export_yup=True)
 tri=sum(len(f.vertices)-2 for f in merged.data.polygons)
 bpy.data.objects.remove(merged,do_unlink=True)
 # Normalize editable originals too, with the same authored center and scale.
 for o in parts:
  world=o.matrix_world.copy()
  for v in o.data.vertices:v.co=(world@v.co-center)*factor
  o.matrix_world=Matrix.Identity(4)
 select(parts)
 scene['slug']=slug
 records[slug]={'slug':slug,'scene':scene.name,'collection':coll.name,'parts':[o.name for o in parts], 'triangles':tri,'max_extent':1.3}
 bpy.data.libraries.write(str(ART/(slug+'.blend')),{scene},fake_user=True)
 (ART/'manifest.json').write_text(json.dumps({'revision':2,'license':'Original game art','models':list(records.values())},indent=2))
 print(slug,':',len(parts),'editable parts,',tri,'triangles')
 return records[slug]

# Individually designed models, with named functional components.
def acorn():
 lathe('Tapered amber nut',[(.02,.018),(.075,.13),(.20,.26),(.41,.34),(.61,.345),(.73,.29),(.77,.23)],WOOD,24,ratio=.90)
 lathe('Acorn shoulder lip',[(.63,.31),(.67,.375),(.73,.38),(.79,.32),(.85,.19),(.86,.04)],DARKWOOD,24,ratio=.94)
 # Overlapping pointed scales follow the cup surface in offset rows.
 for row,z in enumerate([.70,.775]):
  for i in range(12):
   a=(i+row*.5)*math.tau/12;r=.355 if row==0 else .285
   o=prism('Cap scale', [(-.065,-.035),(0,-.085),(.065,-.035),(.058,.035),(0,.06),(-.058,.035)],.028,LIGHTWOOD if (i+row)%3==0 else DARKWOOD,bevel=.008)
   # Original scale profile x/z around zero, stand tangential on cap.
   o.rotation_euler=(0,0,a-math.pi/2);o.location=(r*math.cos(a),r*.94*math.sin(a),z)
 tube('Bent woody stem',[(.0,0,.83),(.015,.0,.98),(.09,.0,1.06),(.14,.0,1.08)],[.065,.06,.045,.018],DARKWOOD,10)
 leaf('Oak sprout',(.035,.0,.96),(-.28,-.015,1.08),.10,GREEN,.04)
 rod('Leaf vein',(.02,-.025,.97),(-.25,-.025,1.075),.010,LIME)
 # Two subtle broad vertical grain marks, rather than a painted stripe on every face.
 for a in [math.pi*1.28,math.pi*1.65]:
  pts=[(.337*math.cos(a)*k,.305*math.sin(a)*k,z) for k,z in [(.53,.16),(.89,.31),(1,.48),(.97,.59)]]
  tube('Nut grain',pts,.008,LIGHTWOOD,5)

def crystal_pan():
 lathe('Hollow prospector pan',[(.02,.21),(.035,.32),(.11,.43),(.20,.49),(.24,.49),(.25,.46),(.20,.435),(.10,.34),(.085,.20)],DARKSTEEL,24)
 ring('Rolled steel lip',(0,0,.23),.48,.032,SILVER)
 # Raised sluice ridges actually follow the inner wall.
 for a in [math.pi*.20,math.pi*.48,math.pi*.77,math.pi*1.03]:
  pts=[(r*math.cos(a),r*math.sin(a),z) for r,z in [(.29,.11),(.34,.14),(.39,.18),(.435,.22)]]
  tube('Sluice riffle',pts,.018,STEEL,6)
 # The handle meets the bowl through a cast socket and two rivets.
 box('Handle socket',(.49,0,.15),(.20,.19,.12),STEEL,.03)
 rod('Wooden grip',(.53,0,.15),(.97,0,.15),.072,DARKWOOD)
 ring('Grip ferrule',(.61,0,.15),.075,.018,GOLD,rot=(0,math.pi/2,0))
 box('Grip inlay',(.83,-.06,.17),(.24,.018,.035),LIGHTWOOD,.008)
 for x,y,s in [(-.14,-.09,.09),(.02,.06,.065),(-.23,.12,.055)]:ico('Gold nugget',(x,y,.13),(s,s*.8,s*.7),GOLDLIGHT,1)
 for x,y,h,r,lean in [(.13,-.04,.29,.10,-.25),(-.07,.16,.22,.075,.18)]:
  crystal('Cyan quartz',(x,y,.12),h,r,SKY,CYAN,lean)

def repair_nail():
 # A forged square shank with tapered tip and softened asymmetric head.
 prism('Forged tapered shank',[(-.07,.24),(-.045,.06),(.005,-.02),(.06,.25),(.085,.91),(-.065,.91)],.12,STEEL,bevel=.012)
 box('Hammered square head',(.01,0,.96),(.42,.33,.12),SILVER,.035,rotation=(.04,-.07,.07))
 box('Head inset',(.01,-.01,1.026),(.26,.22,.012),STEEL,.025)
 # A chisel-cut cross and visible peened corners.
 box('Head slot',(.01,-.01,1.038),(.20,.025,.008),DARKSTEEL,.003)
 box('Repair cross',(.01,-.01,1.039),(.025,.15,.009),DARKSTEEL,.003)

 # Copper collar is an intentional inventory accent, seated below the head.
 box('Copper shoulder',(.009,0,.855),(.16,.16,.055),GOLD,.014)

def lumber_boot():
 # Cross sections create a continuous, rounded leather last instead of stacked boxes.
 rings=[(.05,.27,.44,.10),(.13,.29,.45,.10),(.19,.275,.43,.10),(.28,.26,.43,.12),(.40,.235,.36,.07),(.53,.205,.23,-.08),(.90,.195,.20,-.10),(.98,.21,.21,-.10)]
 # Boot faces front (-Y); long rounded toe projects out of the ankle.
 def boot_shell(name,rs,color):
  seg=16;v=[]
  for z,rx,ry,cy in rs:
   for i in range(seg):
    a=i*math.tau/seg;v.append((rx*math.cos(a),-(ry*math.sin(a)+cy),z))
  faces=[tuple(reversed(range(seg))),tuple(range((len(rs)-1)*seg,len(rs)*seg))]
  for k in range(len(rs)-1):
   for i in range(seg):faces.append((k*seg+i,k*seg+(i+1)%seg,(k+1)*seg+(i+1)%seg,(k+1)*seg+i))
  faces=[tuple(reversed(face)) for face in faces]
  return meshpart(name,v,faces,color,.008,True)
 boot_shell('Layered rubber sole',rings[:3],INK)
 boot_shell('Continuous shaped leather',rings[2:],DARKWOOD)
 # Broad toe cap, heel support and a rolled open cuff.
 capverts=[]
 for z,rx,ry,cy in [(.205,.273,.43,.103),(.28,.26,.43,.12),(.35,.245,.389,.091),(.405,.233,.35,.064)]:
  for j in range(13):
   a=math.pi*(.12+.76*j/12);capverts.append(((rx+.008)*math.cos(a),-((ry+.008)*math.sin(a)+cy),z))
 faces=[]
 for k in range(3):
  for j in range(12):a=k*13+j;faces.append((a+13,a+14,a+1,a))
 meshpart('Fitted leather toe cap',capverts,faces,WOOD,0,True)
 tube('Toe cap seam',capverts[-13:],.008,LIGHTWOOD,5)
 box('Heel reinforcement',(0,.25,.22),(.39,.16,.25),EDGE,.035)
 lathe('Rolled leather cuff',[(.91,.20),(.95,.235),(1.02,.235),(1.045,.20),(1.02,.165),(.945,.165)],LIGHTWOOD,20,center=(0,.10,0),ratio=1.08)
 cyl('Dark boot opening',(0,.10,.976),.166,.016,EDGE,20)
 box('Lace tongue',(0,-.16,.68),(.24,.06,.46),WOOD,.04,rotation=(.15,0,0))
 for i,z in enumerate([.53,.64,.75,.86]):
  y=-.20+(z-.53)*.12
  for side in [-1,1]:screw('Brass eyelet',(side*.115,y-.016,z),.024,GOLD)
  rod('Crossed leather lace',(-.10,y-.035,z-.025),(.10,y-.035,z+.027),.017,PARCHMENT)
  rod('Crossed leather lace',(-.10,y-.034,z+.027),(.10,y-.034,z-.025),.017,PARCHMENT)
 for side in [-1,1]:
  for i in range(5):box('Sole tread',(side*.205,-.25+i*.13,.07),(.11,.052,.042),DARKSTEEL,.01)


def abacus():
 # Slightly splayed uprights and a shaped crest, pegs visibly seat the rods.
 for side in [-1,1]:
  o=lathe('Turned oak upright',[(.04,.075),(.10,.09),(.16,.06),(.86,.06),(.91,.095),(.98,.08)],DARKWOOD,12,center=(side*.42,0,0))
  box('Upright inlay',(side*.42,-.07,.55),(.035,.016,.52),LIGHTWOOD,.008)
 prism('Carved arch crest',[(-.51,.94),(-.51,1.02),(-.35,1.04),(-.23,1.12),(.23,1.12),(.35,1.04),(.51,1.02),(.51,.94)],.18,WOOD,bevel=.024)
 box('Lower oak rail',(0,0,.08),(1.01,.19,.13),WOOD,.025)
 box('Counting separator',(0,0,.36),(.82,.13,.055),LIGHTWOOD,.012)
 for row,z in enumerate([.23,.49,.68,.86]):
  rod('Steel counting rail',(-.42,0,z),(.42,0,z),.021,SILVER)
  for j,x in enumerate([-.28,-.13,.02,.23] if row%2==0 else [-.29,-.06,.09,.24]):
   o=lathe('Shaped counting bead',[(-.064,.018),(-.055,.06),(-.025,.085),(.025,.085),(.055,.06),(.064,.018)],GOLDLIGHT if row==0 else SKY if row%2 else LIGHTWOOD,12)
   o.rotation_euler[1]=math.pi/2;o.location=(x,0,z)
 for x in [-.43,.43]:screw('Oak joint pin',(x,-.106,.98),.033,GOLD)
 # Embossed small rune belongs to the frame, with a darker relief shadow.
 prism('Crest diamond',[(-.055,1.015),(0,1.08),(.055,1.015),(0,.96)],.023,GOLDLIGHT,y=-.105,bevel=.006)

def crystal(name,p,height,r,body=SKY,tip=CYAN,lean=0):
 o=lathe(name,[(0,r*.7),(.12*height,r),(.72*height,r),(.96*height,r*.18),(height,.001)],body,6,smooth=False)
 o.location=p;o.rotation_euler[1]=lean
 # One brighter elongated face retains a faceted gem read.
 v=[(-r*.45,-r*.88,.16*height),(r*.45,-r*.88,.16*height),(r*.45,-r*.88,.71*height),(0,-r*.5,.91*height),(-r*.45,-r*.88,.71*height)]
 f=meshpart(name+' bright facet',v,[(0,1,2,3,4)],tip)
 f.location=p;f.rotation_euler[1]=lean

def weapon_token():
 # Thick chamfered military medallion, raised shield and crossed real sword silhouettes.
 cyl('Bronze medallion',(0,0,.52),.43,.105,GOLD,24,(math.pi/2,0,0),bevel=.022)
 cyl('Recessed enamel field',(0,-.065,.52),.355,.024,DARKRED,24,(math.pi/2,0,0),bevel=.010)
 ring('Coin rolled rim',(0,-.078,.52),.389,.019,GOLDLIGHT,(math.pi/2,0,0))
 for i in range(12):
  a=i*math.tau/12;ball('Rim stud',(.38*math.cos(a),-.092,.52+.38*math.sin(a)),(.015,.009,.015),GLOW,8,4)
 for side in [-1,1]:
  # Blade geometry is tapered and has a distinct hilt/crossguard.
  points=[(-.030,-.27),(-.035,.23),(0,.34),(.035,.23),(.030,-.27)]
  blade=prism('Crossed steel sword',points,.035,SILVER,y=-.112,bevel=.007)
  blade.rotation_euler[1]=side*.62;blade.location.z=.52
  angle=side*.62
  # Transform local hilt parts together around the same center.
  for name,p,size,c in [('Gold guard',(0,-.12,-.17),(.21,.055,.045),GOLDLIGHT),('Sword grip',(0,-.12,-.245),(.052,.055,.13),DARKWOOD)]:
   o=box(name,p,size,c,.01);o.location=Matrix.Rotation(angle,4,'Y')@o.location+Vector((0,0,.52));o.rotation_euler[1]=angle
 shield=[(-.12,.61),(.12,.61),(.105,.46),(0,.385),(-.105,.46)]
 prism('Central shield',shield,.05,GOLD,y=-.16,bevel=.015)
 prism('Shield blue inset',[(-.075,.58),(.075,.58),(.065,.475),(0,.43),(-.065,.475)],.015,BLUE,y=-.194,bevel=.008)
 # Real chain eye and a small leather suspension loop.
 ring('Medallion eye',(0,0,.965),.083,.026,GOLD,(math.pi/2,0,0))
 tube('Leather lanyard',[(-.047,0,1.02),(-.06,0,1.14),(0,0,1.17),(.06,0,1.14),(.047,0,1.02)],.024,DARKWOOD,8)
 # Back maker seal: a readable large diamond with a border.
 prism('Reverse seal',[(-.16,.52),(0,.72),(.16,.52),(0,.32)],.02,BRONZE,y=.065,bevel=.018)

BUILDERS={'acorn':acorn,'crystal_pan':crystal_pan,'repair_nail':repair_nail,'lumber_boot':lumber_boot,'abacus':abacus,'weapon_token':weapon_token}

# REFINEMENT STAGE TWO

def idle_spring():
 # Continuous swept wire, closed ground end turns and a warm mechanical collar.
 turns=4.35;n=136
 points=[]
 for i in range(n):
  t=i/(n-1);a=t*math.tau*turns
  z=.17+.72*t
  # Ground ends taper their rise into the bearing seats.
  r=.235+.018*math.sin(t*math.pi)
  points.append((r*math.cos(a),r*math.sin(a),z))
 tube('Continuous helical steel spring',points,.044,SILVER,8)
 for z in [.10,.96]:
  lathe('Machined end seat',[(z-.055,.25),(z-.04,.30),(z+.025,.30),(z+.055,.265)],DARKSTEEL,20)
  cyl('Brass retaining band',(0,0,z+.025),.312,.023,GOLD,24,bevel=.003)
  cyl('Bearing recess',(0,0,z+.058),.19,.012,INK,20)
  ring('Inner seat lip',(0,0,z+.065),.186,.016,STEEL)
 # A narrow keeper plate reads as a loaded mechanical part.
 box('Spring keeper',(.257,.02,.51),(.065,.13,.47),GOLD,.018)
 for z in [.32,.70]:screw('Keeper screw',(.259,-.053,z),.026,WHITE)

def repair_magnet():
 # An actual open horseshoe with a continuous rounded heel.
 points=[(-.37,1.02),(-.37,.39)]
 for i in range(15):
  a=math.pi+i*math.pi/14;points.append((.37*math.cos(a),.39+.31*math.sin(a)))
 points +=[(.37,1.02),(.16,1.02),(.16,.39)]
 for i in range(15):
  a=math.tau-i*math.pi/14;points.append((.16*math.cos(a),.39+.115*math.sin(a)))
 points +=[(-.16,1.02)]
 prism('Forged horseshoe body',points,.27,DARKRED,bevel=.025)
 # Enamel follows both arms rather than two unrelated colored pillars.
 for side,c in [(-1,RED),(1,BLUE)]:
  box('Enamel arm',(side*.265,-.142,.70),(.175,.022,.47),c,.024)
  box('Pole steel cap',(side*.265,0,1.025),(.24,.31,.19),SILVER,.025)
  box('Pole dark separator',(side*.265,0,.925),(.235,.30,.034),INK,.007)
  box('Pole face shine',(side*.265,-.165,1.055),(.14,.012,.035),WHITE,.005)
 for side in [-1,1]:screw('Magnet assembly screw',(side*.265,-.164,.49),.027,GOLD)
 # Small repair emblem on the seated heel plate.
 cyl('Heel brass badge',(0,-.15,.22),.115,.035,GOLD,16,(math.pi/2,0,0))
 box('Badge cross upright',(0,-.179,.22),(.035,.015,.15),GLOW,.005)
 box('Badge cross bar',(0,-.179,.22),(.13,.015,.035),GLOW,.005)

def siren():
 lathe('Cast alarm pedestal',[(.03,.29),(.06,.37),(.14,.37),(.18,.31),(.23,.31)],INK,20)
 ring('Pedestal brass rim',(0,0,.16),.325,.024,GOLD)
 lathe('Red beacon lens',[(.21,.27),(.28,.29),(.57,.27),(.70,.21),(.76,.10),(.77,.015)],RED,24)
 # Surface highlight is a curved inset strip, no disconnected rectangle.
 tube('Beacon lens gleam',[(-.15,-.22,.33),(-.16,-.235,.48),(-.13,-.22,.60)],[.022,.027,.012],ORANGE,6)
 for z in [.25,.43,.61]:ring('Lens fluting',(0,0,z),.279 if z<.5 else .242,.010,DARKRED)
 # Four protective cage bars, attached to the metal base and crown.
 for i in range(4):
  a=math.pi/4+i*math.tau/4
  tube('Alarm guard',[(.32*math.cos(a),.32*math.sin(a),.18),(.33*math.cos(a),.33*math.sin(a),.48),(.21*math.cos(a),.21*math.sin(a),.76),(0,0,.83)],.018,DARKSTEEL,6)
 cyl('Crown cap',(0,0,.81),.09,.055,GOLD,12)
 for x in [-.23,.23]:
  screw('Base bolt',(x,-.29,.11),.028,STEEL)
 # A small horn provides the missing sound-producing function.
 horn=lathe('Warning horn',[(0,.065),(.11,.07),(.23,.17),(.25,.18),(.27,.17),(.24,.135),(.14,.05)],GOLD,16)
 horn.rotation_euler[0]=math.pi/2;horn.location=(0,-.20,.22)
 cyl('Horn dark throat',(0,-.40,.22),.09,.012,INK,16,(math.pi/2,0,0))

def clover():
 # Four heart-shaped leaves with actual notches and raised veins.
 center=Vector((0,0,.73))
 tube('Bent clover stem',[(.015,0,.02),(-.07,.01,.22),(-.055,0,.43),(0,0,.73)],[.032,.036,.032,.025],DARKGREEN,8)
 for k,a in enumerate([math.pi/4,3*math.pi/4,5*math.pi/4,7*math.pi/4]):
  axis=Vector((math.cos(a),0,math.sin(a)));side=Vector((-math.sin(a),0,math.cos(a)))
  # Heart boundary starts at its point; the two lobes flank a shallow notch.
  shape=[]
  for j in range(32):
   t=math.pi+j*math.tau/32
   shape.append((.011*16*math.sin(t)**3,.014*(17+13*math.cos(t)-5*math.cos(2*t)-2*math.cos(3*t)-math.cos(4*t))))
  boundary=[center+axis*v+side*u+Vector((0,-.025-.025*math.sin(v*math.pi/.43),0)) for u,v in shape]
  raised=center+axis*.23+Vector((0,-.10,0))
  verts=[tuple(raised)]+[tuple(v) for v in boundary]
  faces=[(0,i+1,(i+1)%len(boundary)+1) for i in range(len(boundary))]
  o=meshpart('Heart-shaped clover leaf',verts,faces,GREEN if k%2 else LIME,0,True)
  select([o]);m=o.modifiers.new('Fleshy leaf thickness','SOLIDIFY');m.thickness=.035;bpy.ops.object.modifier_apply(modifier=m.name)
  tube('Central leaf vein',[tuple(center+Vector((0,-.035,0))),tuple(raised+Vector((0,-.006,0))),tuple(center+axis*.36+Vector((0,-.045,0)))],.011,DARKGREEN,5)
 ball('Clover heart',(0,-.065,.73),(.058,.035,.058),GOLDLIGHT,12,6)
 # One smaller leaf makes the stem feel grown rather than a straight handle.
 leaf('Stem leaf',(-.06,0,.34),(-.23,-.01,.48),.055,GREEN,.025)

def mining_hammer():
 # Hand-forged double-ended mining hammer with a wedged chisel end.
 shaft=lathe('Tapered hickory handle',[(.04,.065),(.10,.085),(.26,.075),(.70,.055),(.94,.065)],DARKWOOD,12)
 for i in range(7):
  z=.12+i*.052;ring('Leather grip wrap',(0,0,z),.079-i*.001,.014,WOOD)
 cyl('Grip pommel',(0,0,.066),.094,.075,STEEL,12,bevel=.012)
 head=[(-.51,.85),(-.51,1.08),(-.37,1.12),(.22,1.10),(.48,.98),(.53,.87),(.37,.91),(.20,.93),(-.33,.91)]
 prism('Forged asymmetric hammer head',head,.31,DARKSTEEL,bevel=.035)
 box('Hardened striking face',(-.47,0,.975),(.13,.36,.28),SILVER,.035,rotation=(0,-.02,0))
 # Raised central strap passes around both faces and anchors the shaft.
 box('Head brass saddle',(0,0,.99),(.18,.34,.27),GOLD,.025)
 for y in [-.183,.183]:
  box('Saddle inset',(0,y,1.005),(.085,.022,.15),BRONZE,.012)
  ball('Steel wedge rivet',(0,y*1.08,1.005),(.026,.015,.026),SILVER,10,5)
 rod('Head wedge',(0,.0,1.10),(.07,.0,1.17),.045,DARKWOOD)
 # Two broad bevel chips are readable while not filling the tool with noise.
 tube('Striking face scar',[(-.531,-.13,1.035),(-.535,-.02,1.01),(-.531,.09,1.025)],.008,DARKSTEEL,5)
 ring('Lanyard ring',(0,0,.005),.062,.017,GOLD,(math.pi/2,0,0))

def dawn_coin():
 cyl('Thick dawn coin',(0,0,.59),.46,.11,GOLD,32,(math.pi/2,0,0),bevel=.022)
 for side in [-1,1]:
  cyl('Coin inset face',(0,side*.064,.59),.382,.016,BRONZE,32,(math.pi/2,0,0),bevel=.008)
  ring('Bright struck rim',(0,side*.08,.59),.416,.021,GOLDLIGHT,(math.pi/2,0,0))
  y=side*.083
  # Raised rising sun with radial tapered rays above a horizon.
  sun=[(.165*math.cos(math.pi-i*math.pi/16),.50+.165*math.sin(math.pi-i*math.pi/16)) for i in range(17)]
  prism('Rising half sun',sun,.035,GOLDLIGHT,y=y,bevel=.009)
  prism('Horizon line',[(-.30,.50),(.30,.50),(.30,.46),(-.30,.46)],.022,GOLDLIGHT,y=y,bevel=.008)
  for i in range(7):
   a=i*math.pi/6;v=Vector((math.cos(a),math.sin(a)));perp=Vector((-v.y,v.x))
   points=[tuple(Vector((0,.59))+v*.23-perp*.017),tuple(Vector((0,.59))+v*.33),tuple(Vector((0,.59))+v*.23+perp*.017)]
   prism('Tapered sun ray',points,.018,GLOW,y=y,bevel=.005)
  for z,w in [(.405,.21),(.35,.12)]:box('Engraved dawn waves',(0,y,z),(w,.025,.018),GOLDLIGHT,.005)
 # Edge milling catches the light during pickup spin.
 for i in range(20):
  a=i*math.tau/20
  o=box('Coin edge flute',(.451*math.cos(a),0,.59+.451*math.sin(a)),(.020,.065,.038),BRONZE,.006)
  o.rotation_euler[1]=-a

def ghost_candle():
 # Dish, pedestal and finger loop form an actual portable candlestick.
 lathe('Brass saucer',[(.015,.11),(.035,.28),(.065,.34),(.105,.34),(.13,.31),(.095,.25),(.075,.11)],GOLD,24)
 lathe('Candle socket',[(.075,.14),(.16,.15),(.19,.19),(.23,.19),(.25,.155)],BRONZE,20)
 ring('Socket rolled rim',(0,0,.23),.176,.019,GOLDLIGHT)
 # Irregular crown is real wax geometry.
 n=20;verts=[]
 for z,r in [(.20,.15),(.72,.153)]:
  for i in range(n):
   a=i*math.tau/n;zz=z if z<.5 else z+.025*math.sin(3*a)+.016*math.sin(7*a)
   verts.append((r*math.cos(a),r*math.sin(a),zz))
 faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
 for i in range(n):faces.append((i,(i+1)%n,(i+1)%n+n,i+n))
 meshpart('Uneven lilac wax',verts,faces,LILAC,0,True)
 cyl('Recessed wax pool',(0,0,.735),.115,.012,PURPLE,20)
 for a,length in [(-1.9,.23),(-.9,.15),(.2,.12),(1.4,.18)]:
  r=.153
  tube('Melted wax run',[(r*math.cos(a),r*math.sin(a),.72),(r*1.02*math.cos(a),r*1.02*math.sin(a),.66),(r*1.015*math.cos(a),r*1.015*math.sin(a),.72-length)],[.035,.029,.012],LILAC,8)
 tube('Blackened wick',[(0,0,.72),(.01,0,.82),(.035,0,.85)],.017,INK,6)
 # Swept teardrop flame, tapering in a breeze rather than a sphere on a rod.
 flame=[(0,.0,.79),(-.025,.0,.86),(.0,.0,.97),(.06,.0,1.10),(.055,.0,1.18)]
 tube('Golden teardrop flame',flame,[.020,.085,.075,.035,.001],GOLDLIGHT,12)
 tube('Warm flame heart',[(0,-.043,.82),(.005,-.06,.90),(.03,-.03,1.02)],[.018,.040,.001],ORANGE,10)
 ring('Candlestick finger loop',(.285,0,.16),.115,.026,GOLD,(math.pi/2,0,0))
 for i in range(3):
  a=math.pi/6+i*math.tau/3;ball('Saucer foot',(.18*math.cos(a),.18*math.sin(a),.018),(.055,.055,.025),BRONZE,10,5)

def core_fuse():
 # Thick ceramic cartridge with stepped brass contacts and screw-on vented caps.
 lathe('Ivory ceramic fuse body',[(.16,.16),(.20,.19),(.75,.19),(.79,.16)],IVORY,20)
 for z in [.13,.82]:
  lathe('Machined brass terminal',[(z-.075,.18),(z-.055,.23),(z+.05,.23),(z+.075,.19)],GOLD,20)
  ring('Terminal bright shoulder',(0,0,z+.041),.223,.015,GOLDLIGHT)
  for i in range(8):
   a=i*math.tau/8
   box('Terminal notch',(.224*math.cos(a),.224*math.sin(a),z),(.035,.035,.065),BRONZE,.004,rotation=(0,0,a))
 # The visible copper safety link follows a black recessed ceramic channel.
 box('Fuse inset channel',(0,-.188,.48),(.20,.020,.39),DARKSTEEL,.025)
 tube('Copper safety wire',[(-.06,-.207,.33),(.055,-.207,.41),(-.055,-.207,.54),(.06,-.207,.64)],.018,ORANGE,6)
 for z in [.31,.67]:screw('Wire terminal screw',(0,-.211,z),.030,GOLDLIGHT)
 prism('Warning bolt',[(-.025,.57),(.07,.57),(.015,.47),(.055,.47),(-.055,.35),(-.02,.46),(-.065,.46)],.020,GOLDLIGHT,y=.197,bevel=.006)
 for z in [.02,.94]:box('Electrical contact blade',(0,0,z),(.24,.075,.12),SILVER,.018)

BUILDERS.update({'idle_spring':idle_spring,'repair_magnet':repair_magnet,'siren':siren,'clover':clover,'mining_hammer':mining_hammer,'dawn_coin':dawn_coin,'ghost_candle':ghost_candle,'core_fuse':core_fuse})

# REFINEMENT STAGE THREE

def crystal_safe():
 box('Cast iron safe shell',(0,.035,.57),(.82,.63,.92),INK,.075)
 # Back panel, protective corners, dimensional inset door and visible hinge barrels.
 box('Safe rear plate',(0,.36,.57),(.66,.035,.74),DARKSTEEL,.04)
 box('Safe door rim',(0,-.315,.57),(.75,.12,.85),BRONZE,.060)
 box('Recessed blue steel door',(0,-.39,.57),(.65,.06,.73),DARKSTEEL,.045)
 box('Door inner field',(0,-.426,.57),(.52,.018,.59),STEEL,.033)
 for side in [-1,1]:
  for z in [.18,.97]:box('Brass corner armor',(side*.34,-.01,z),(.19,.65,.15),GOLD,.035)
 for z in [.35,.77]:
  cyl('Door hinge barrel',(-.36,-.33,z),.045,.21,SILVER,12,bevel=.01)
  for dz in [-.075,.075]:ring('Hinge seam',(-.36,-.33,z+dz),.046,.009,INK)
 # Wheel axle sits in the lock plate; spokes meet a continuous handwheel.
 cyl('Lock boss',(.04,-.46,.60),.115,.06,INK,20,(math.pi/2,0,0))
 ring('Combination handwheel',(.04,-.52,.60),.155,.026,GOLDLIGHT,(math.pi/2,0,0))
 cyl('Wheel axle',(.04,-.53,.60),.047,.045,GOLD,12,(math.pi/2,0,0))
 for i in range(3):
  a=i*math.tau/3+.18;rod('Handwheel spoke',(.04,-.53,.60),(.04+.15*math.cos(a),-.53,.60+.15*math.sin(a)),.019,GOLD)
 for x in [-.255,.255]:
  for z in [.28,.86]:screw('Door screw',(x,-.45,z),.026,SILVER)
 # Crystal insignia in a recessed diamond setting, rather than a loose cone.
 prism('Crystal badge socket',[(.18,.34),(.25,.44),(.32,.34),(.25,.25)],.026,INK,y=-.447,bevel=.009)
 prism('Cyan crystal insignia',[(.195,.34),(.25,.415),(.305,.34),(.25,.275)],.018,CYAN,y=-.468,bevel=.005)
 for x in [-.25,.25]:box('Safe foot',(x,.03,.035),(.16,.40,.12),DARKSTEEL,.025)
 # Back strengthening ribs show a finished reverse face.
 for x in [-.20,.20]:box('Rear reinforcement',(x,.383,.57),(.06,.018,.55),STEEL,.012)

def rebuild_blueprint():
 # Bowed parchment sheet with two curled scroll ends. Drawing is raised blue/white ink geometry.
 n=16;verts=[]
 for side in [0,1]:
  for j in range(9):
   z=.16+j*.10
   for i in range(n+1):
    x=-.36+i*.72/n;y=.035*math.cos(x*math.pi/.72)+.012*math.sin(z*4)+(side-.5)*.024
    verts.append((x,y,z))
 width=n+1;layer=9*width;faces=[]
 for j in range(8):
  for i in range(n):
   a=j*width+i;faces.append((a,a+width,a+width+1,a+1));a+=layer;faces.append((a,a+1,a+width+1,a+width))
 for i in range(n):faces +=[(i,i+1,i+1+layer,i+layer),(8*width+i,8*width+i+layer,8*width+i+1+layer,8*width+i+1)]
 for j in range(8):
  a=j*width;faces.append((a,a+layer,a+width+layer,a+width));a+=n;faces.append((a,a+width,a+width+layer,a+layer))
 faces=[tuple(reversed(face)) for face in faces]
 meshpart('Gently bowed blueprint',verts,faces,BLUE,0,True)
 for x in [-.405,.405]:
  lathe('Rolled scroll end',[(.10,.065),(.14,.083),(.94,.083),(1.0,.06)],SKY,16,center=(x,.015,0))
  for z in [.10,1.0]:cyl('Scroll end paper',(x,.015,z),.055,.014,IVORY,16)
 # Subtle grid, with a much bolder fort/turret plan.
 for x in [-.27,-.135,0,.135,.27]:box('Blueprint grid',(x,-.001,.55),(.006,.007,.69),SKY,.001)
 for z in [.25,.40,.55,.70,.85]:box('Blueprint grid',(0,-.001,z),(.59,.007,.006),SKY,.001)
 y=-.025
 for x in [-.14,.14]:box('Tower plan column',(x,y,.57),(.026,.018,.31),WHITE,.003)
 for z,w in [(.41,.36),(.74,.40),(.80,.30)]:box('Tower plan line',(0,y,z),(w,.018,.028),WHITE,.003)
 tube('Cannon diagram barrel',[(.0,y-.005,.78),(.23,y-.005,.91)],.020,WHITE,6)
 for x in [-.25,.25]:
  rod('Dimension arrow',(x,y,.37),(x,y,.77),.006,GOLDLIGHT)
  for z,sign in [(.37,1),(.77,-1)]:
   rod('Arrowhead',(x,y,z),(x-.025,y,z+sign*.04),.006,GOLDLIGHT)
   rod('Arrowhead',(x,y,z),(x+.025,y,z+sign*.04),.006,GOLDLIGHT)
 # Ink geometry follows the same bowed surface as the parchment, with no hovering lines.
 for obj in parts:
  if not obj.name.startswith(('Blueprint grid','Tower plan','Cannon diagram','Dimension arrow','Arrowhead')):continue
  world=obj.matrix_world.copy();coords=[world@v.co for v in obj.data.vertices]
  cy=(min(v.y for v in coords)+max(v.y for v in coords))/2
  for vertex,point in zip(obj.data.vertices,coords):
   point.y=.035*math.cos(point.x*math.pi/.72)+.012*math.sin(point.z*4)-.012-.007+(point.y-cy)*.30
   vertex.co=point
  obj.matrix_world=Matrix.Identity(4)
 # Rolled parchment back and attached architect's pencil along the side.
 rod('Architect pencil',(.50,-.02,.20),(.50,-.02,.91),.035,WOOD)
 cyl('Pencil sharpened tip',(.50,-.02,.155),.0,.095,IVORY,8,r2=.036,bevel=0)
 cyl('Graphite tip',(.50,-.02,.11),.0,.04,INK,8,r2=.012,bevel=0)
 for z in [.31,.78]:box('Pencil leather loop',(.50,-.02,z),(.115,.095,.038),DARKWOOD,.01)
 # Reverse ribbon makes the backside distinct without duplicating the drawing.
 box('Scroll reverse binding',(0,.062,.53),(.15,.035,.75),DARKWOOD,.018)
 box('Binding clasp',(0,.09,.57),(.19,.05,.14),GOLD,.018)

def lucky_die():
 # A solid ivory die with true inset pips on all six faces, opposite faces total seven.
 die=box('Rounded ivory die',(0,0,.58),(.76,.76,.76),IVORY,.085)
 positions={1:[(0,0)],2:[(-.18,-.18),(.18,.18)],3:[(-.18,-.18),(0,0),(.18,.18)],4:[(-.18,-.18),(-.18,.18),(.18,-.18),(.18,.18)],
 5:[(-.18,-.18),(-.18,.18),(0,0),(.18,-.18),(.18,.18)],6:[(-.18,-.20),(-.18,0),(-.18,.20),(.18,-.20),(.18,0),(.18,.20)]}
 center=Vector((0,0,.58))
 for normal,u,v,count in [((0,-1,0),(1,0,0),(0,0,1),5),((0,1,0),(-1,0,0),(0,0,1),2),((1,0,0),(0,1,0),(0,0,1),3),((-1,0,0),(0,-1,0),(0,0,1),4),((0,0,1),(1,0,0),(0,1,0),1),((0,0,-1),(1,0,0),(0,-1,0),6)]:
  normal,u,v=Vector(normal),Vector(u),Vector(v)
  for a,b in positions[count]:
   p=center+normal*.379+u*a+v*b
   # Shallow actual circular pocket, filled with dark enamel at its bottom.
   bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.053,depth=.052,location=p)
   cutter=bpy.context.object;cutter.name='Pip pocket cutter';cutter.rotation_euler=normal.to_track_quat('Z','Y').to_euler()
   select([die]);mod=die.modifiers.new('Carved pip pocket','BOOLEAN');mod.operation='DIFFERENCE';mod.object=cutter
   bpy.ops.object.modifier_apply(modifier=mod.name);bpy.data.objects.remove(cutter,do_unlink=True)
   p2=p-normal*.024
   pip=cyl('Inset enamel pip',p2,.047,.010,INK,12,bevel=.002);pip.rotation_euler=normal.to_track_quat('Z','Y').to_euler()
 die.data.update()
 die.data.normals_split_custom_set([(0.0,0.0,0.0)]*len(die.data.loops))
 for face in die.data.polygons:
  if max(abs(c) for c in face.normal)>.999:face.use_smooth=False
 die.data.update()
 explicit=[tuple(n.vector) for n in die.data.corner_normals]
 for face in die.data.polygons:
  if not face.use_smooth:
   for i in face.loop_indices:explicit[i]=tuple(face.normal)
 die.data.normals_split_custom_set(explicit)
 # Single warm accent follows one rounded edge, restrained rather than a generic gem stuck on.
 # No extra ornament changes the familiar die silhouette.

def upgrade_mirror():
 # A sculpted, arched oval hand mirror, thick rolled metal rim and a separate backing.
 def oval(rx,rz,z=.71,n=40):return [(rx*math.cos(i*math.tau/n),z+rz*math.sin(i*math.tau/n)) for i in range(n)]
 prism('Mirror dark wood backing',oval(.335,.405),.12,DARKWOOD,y=.018,bevel=.025)
 prism('Mirror recessed gold surround',oval(.36,.43),.10,GOLD,y=-.055,bevel=.022)
 prism('Dark glass reveal',oval(.298,.36),.027,BRONZE,y=-.118,bevel=.012)
 prism('Polished azure glass',oval(.274,.334),.018,CYAN,y=-.14,bevel=.014)
 # Two broad controlled highlights follow the surface rather than a long flat label.
 prism('Glass long glint',[(-.19,.59),(-.15,.96),(-.10,1.00),(-.13,.60)],.007,WHITE,y=-.154,bevel=.009)
 prism('Glass short glint',[(.13,.45),(.16,.69),(.19,.72),(.17,.47)],.007,SKY,y=-.154,bevel=.008)
 # Swept rim and scroll-shaped crown create a recognisable silhouette.
 tube('Bright oval bezel',[(.328*math.cos(i*math.tau/40),-.12,.71+.397*math.sin(i*math.tau/40)) for i in range(40)],.022,GOLDLIGHT,8,True)
 for side in [-1,1]:
  tube('Crown scroll',[(side*.11,-.055,1.085),(side*.15,-.055,1.17),(side*.075,-.055,1.205),(side*.025,-.055,1.16)],.026,GOLD,8)
 prism('Crown violet jewel',[(-.053,1.12),(0,1.21),(.053,1.12),(0,1.055)],.038,VIOLET,y=-.097,bevel=.009)
 lathe('Turned walnut handle',[(.02,.075),(.06,.089),(.12,.062),(.26,.055),(.33,.083),(.38,.073)],DARKWOOD,16)
 for z in [.07,.28]:ring('Handle gold collar',(0,0,z),.071,.017,GOLDLIGHT)
 # Ornate back plate: an embossed sunburst attached to the wooden backing.
 cyl('Reverse maker seal',(0,.09,.71),.12,.026,GOLD,16,(math.pi/2,0,0))
 for i in range(6):
  a=i*math.tau/6;rod('Reverse sun ray',(.15*math.cos(a),.095,.71+.15*math.sin(a)),(.22*math.cos(a),.095,.71+.22*math.sin(a)),.012,BRONZE)

BUILDERS.update({'crystal_safe':crystal_safe,'rebuild_blueprint':rebuild_blueprint,'lucky_die':lucky_die,'upgrade_mirror':upgrade_mirror})

def gallery():
 global scene
 scene=bpy.data.scenes.new('Chest Relics Crafted Gallery');bpy.context.window.scene=scene
 order=['acorn','crystal_pan','repair_nail','lumber_boot','abacus','weapon_token','idle_spring','repair_magnet','siren','clover','mining_hammer','dawn_coin','ghost_candle','core_fuse','crystal_safe','rebuild_blueprint','lucky_die','upgrade_mirror']
 for i,slug in enumerate(order):
  record=records[slug]
  collection=bpy.data.collections.new(slug.upper());scene.collection.children.link(collection)
  for name in record['parts']:
   original=bpy.data.objects.get(name)
   if not original:continue
   o=original.copy();o.data=original.data.copy();collection.objects.link(o)
   o.location.x+=(i%6)*1.9;o.location.z+=(2-i//6)*1.8+.7
 scene['authored_runtime']=json.dumps(list(records.values()))
 bpy.data.libraries.write(str(ART/'chest_relics.blend'),{scene},fake_user=True)
 print('Gallery saved:',len(records),'individually authored models')


def clean_mesh(obj):
    """Discard degenerate export triangles without altering split normals or UV seams."""
    old=obj.data;old.calc_loop_triangles()
    triangles=[t for t in old.loop_triangles if t.area>1e-11]
    used=sorted({int(i) for t in triangles for i in t.vertices});mapping={v:i for i,v in enumerate(used)}
    vertices=[tuple(old.vertices[i].co) for i in used]
    faces=[tuple(mapping[int(i)] for i in t.vertices) for t in triangles]
    uvs=[tuple(old.uv_layers.active.data[i].uv) for t in triangles for i in t.loops]
    normals=[tuple(old.corner_normals[i].vector.normalized()) for t in triangles for i in t.loops]
    mesh=bpy.data.meshes.new(obj.name+'_CleanRuntime');mesh.from_pydata(vertices,[],faces);mesh.update()
    layer=mesh.uv_layers.new(name='RelicPalette')
    for i,uv in enumerate(uvs):layer.data[i].uv=uv
    for face in mesh.polygons:face.use_smooth=True
    for material in old.materials:mesh.materials.append(material)
    mesh.normals_split_custom_set(normals)
    obj.data=mesh
    return len(faces)

def clean_existing_exports():
    global scene
    previous=bpy.context.window.scene
    scene=bpy.data.scenes.new('Crafted Runtime Cleanup');bpy.context.window.scene=scene
    for path in sorted(OUT.glob('*.glb')):
        before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(path));objects=[o for o in scene.objects if o not in before]
        obj=next(o for o in objects if o.type=='MESH');obj.name=path.stem+'_runtime'
        count=clean_mesh(obj);select([obj])
        bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,use_active_scene=True,export_yup=True)
        records[path.stem]['triangles']=count
        for o in objects:bpy.data.objects.remove(o,do_unlink=True)
    bpy.context.window.scene=previous
    (ART/'manifest.json').write_text(json.dumps({'revision':2,'license':'Original game art','models':list(records.values())},indent=2))

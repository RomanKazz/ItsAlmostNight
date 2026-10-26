"""Original low-poly Stump character, editable source and true 3D portrait."""
from pathlib import Path
import bpy, math, json
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/stump';ART.mkdir(parents=True,exist_ok=True)
OUT=ROOT/'assets/models/characters';OUT.mkdir(parents=True,exist_ok=True)
ICON=ROOT/'assets/ui/characters';ICON.mkdir(parents=True,exist_ok=True)
scene=bpy.data.scenes.new('STUMP Character');bpy.context.window.scene=scene
collection=bpy.data.collections.new('STUMP_MODEL');scene.collection.children.link(collection)
parts=[]
atlas=bpy.data.images.load(str(ART/'palette.png'),check_existing=False);atlas.pack()
material=bpy.data.materials.new('Stump warm bark and fresh shoots');material.use_nodes=True
node=next(n for n in material.node_tree.nodes if n.type=='BSDF_PRINCIPLED');node.inputs['Roughness'].default_value=.82
texture=material.node_tree.nodes.new('ShaderNodeTexImage');texture.image=atlas
material.node_tree.links.new(texture.outputs['Color'],node.inputs['Base Color'])

def finish(o,name,color,bevel=0):
    o.name=name
    for c in list(o.users_collection):c.objects.unlink(o)
    collection.objects.link(o);bpy.context.view_layer.objects.active=o
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod=o.modifiers.new('Soft carved edge','BEVEL');mod.width=bevel;mod.segments=2
        bpy.ops.object.modifier_apply(modifier=mod.name)
    o.data.materials.clear();o.data.materials.append(material)
    for old in list(o.data.uv_layers):o.data.uv_layers.remove(old)
    uv=o.data.uv_layers.new(name='Packed palette')
    for face in o.data.polygons:
        shade=.62+.25*max(0,face.normal.z)+.10*max(0,-face.normal.y)
        for i in face.loop_indices:uv.data[i].uv=((color+.5)/32,shade)
    parts.append(o);return o

def mesh(name,vertices,faces,color):
    data=bpy.data.meshes.new(name);data.from_pydata(vertices,[],faces);data.update()
    o=bpy.data.objects.new(name,data);collection.objects.link(o);return finish(o,name,color)

def cube(name,p,size,color,bevel=.035,rotation=(0,0,0)):
    bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.scale=size
    finish(o,name,color,bevel);o.rotation_euler=rotation;return o

def ball(name,p,size,color):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=1,location=p);o=bpy.context.object;o.scale=size
    return finish(o,name,color)

def rod(name,a,b,r0,r1,color,vertices=9):
    a,b=Vector(a),Vector(b);delta=b-a
    bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=r0,radius2=r1,depth=delta.length,location=(a+b)/2)
    o=bpy.context.object;o.rotation_euler=delta.to_track_quat('Z','Y').to_euler();return finish(o,name,color)

def ring(name,p,r,thickness,color,rotation=(0,0,0)):
    bpy.ops.mesh.primitive_torus_add(major_segments=28,minor_segments=6,location=p,major_radius=r,minor_radius=thickness,rotation=rotation)
    return finish(bpy.context.object,name,color)

def leaf(name,a,b,width,color):
    a,b=Vector(a),Vector(b);mid=(a+b)/2;axis=b-a
    side=axis.cross(Vector((0,-1,.2))).normalized()*width
    ridge=mid+Vector((0,-.045,.045))
    return mesh(name,[a,mid+side,b,mid-side,ridge],[(0,1,4),(1,2,4),(2,3,4),(3,0,4),(0,3,2,1)],color)

# A broad, slightly crooked trunk with genuine radial bark facets.
n=14;profiles=[(.18,.39),(.35,.50),(.80,.48),(1.19,.54),(1.36,.49)]
vertices=[]
for z,r in profiles:
    for i in range(n):
        angle=i*math.tau/n;radius=r*(1+.055*math.sin(i*7.1))
        vertices.append((radius*math.cos(angle),radius*math.sin(angle),z+.02*math.sin(i*2.3)))
faces=[tuple(reversed(range(n))),tuple(range((len(profiles)-1)*n,len(profiles)*n))]
for layer in range(len(profiles)-1):
    for i in range(n):faces.append((layer*n+i,layer*n+(i+1)%n,(layer+1)*n+(i+1)%n,(layer+1)*n+i))
body=mesh('Ancient crooked trunk',vertices,faces,2)
# Bark plates rise into the cut edge, not a generic cylinder texture.
for i in range(n):
    a=i*math.tau/n;r=.49
    rod('Raised bark rib',(r*math.cos(a),r*math.sin(a),.34),((r+.01)*math.cos(a),(r+.01)*math.sin(a),1.30),.032,.018,1 if i%3 else 3,5)
rod('Pale cut wood',(0,0,1.34),(0,0,1.385),.455,.455,4,28)
for radius in [.13,.25,.36]:ring('Uneven growth ring',(0,0,1.389),radius,.009,2)
rod('Dark heartwood',(0,0,1.385),(0,0,1.397),.055,.055,1,10)
# Root feet and planted stance, each fork actually connected to the trunk.
for side in [-1,1]:
    for j in range(3):
        a=(side*.27,.01,.29);b=(side*(.40+.075*j),-.17+.18*j,.07)
        rod('Root boot',a,b,.15,.09,1);rod('Root toe',b,(b[0]+side*.13,b[1]-.12,.045),.09,.025,2)
    shoulder=(side*.45,0,1.04);elbow=(side*.69,-.02,.79);wrist=(side*.77,-.25,.73)
    rod('Branch upper arm',shoulder,elbow,.17,.15,1);rod('Branch forearm',elbow,wrist,.15,.17,2)
    cube('Massive wooden fist',(side*.79,-.30,.73),(.38,.39,.40),2,.065)
    for k in range(3):
        cube('Carved knuckle',(side*(.68+.095*k),-.505,.79),(.087,.055,.18),3,.018)
    rod('Fist cut end',(side*.79,-.52,.65),(side*.79,-.53,.65),.10,.10,4,10)
# Expressive carved face, recessed sockets and warm living eyes.
for side in [-1,1]:
    ball('Carved eye socket',(side*.195,-.453,1.04),(.145,.073,.15),0)
    ball('Amber eye',(side*.195,-.505,1.055),(.085,.028,.090),12)
    ball('Eye highlight',(side*.17,-.533,1.09),(.022,.008,.025),9)
    cube('Determined bark brow',(side*.19,-.47,1.22),(.30,.12,.075),1,.02,(0,side*.18,side*.11))
mesh('Friendly carved mouth',[(-.12,-.493,.83),(.13,-.493,.83),(.09,-.503,.79),(-.06,-.503,.77)],[(0,1,2,3)],0)
rod('Wooden nose',(0,-.46,.99),(0,-.56,.94),.055,.035,3,6)
# Moss islands and lively shoots make the silhouette unmistakable at icon size.
for side in [-1,1]:
    ball('Shoulder moss',(side*.46,.015,1.13),(.15,.16,.065),20)
    leaf('Shoulder oak leaf',(side*.40,-.06,1.15),(side*.62,-.11,1.28),.075,21)
rod('Bent crown twig',(.20,.02,1.36),(.27,.01,1.59),.037,.024,1)
rod('Twig fork',(.27,.01,1.59),(.44,.025,1.68),.024,.009,2)
leaf('Crown fresh leaf',(.29,.01,1.57),(.08,-.035,1.76),.095,22)
leaf('Crown oak leaf',(.36,.02,1.64),(.60,.015,1.76),.095,21)

# Runtime copy: one mesh/material, original parts remain editable in the source.
for o in bpy.context.view_layer.objects:o.select_set(False)
copies=[]
for original in parts:
    o=original.copy();o.data=original.data.copy();collection.objects.link(o);o.select_set(True);copies.append(o)
bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();runtime=bpy.context.object;runtime.name='STUMP'
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
bpy.ops.export_scene.gltf(filepath=str(OUT/'stump.glb'),export_format='GLB',use_selection=True,use_active_scene=True)
triangles=sum(len(p.vertices)-2 for p in runtime.data.polygons)
bpy.data.objects.remove(runtime,do_unlink=True)
(ART/'manifest.json').write_text(json.dumps({'license':'Original game art','height_m':1.8,'runtime':'assets/models/characters/stump.glb','triangles':triangles,'parts':len(parts),'palette':'chest relics crafted palette, embedded PNG','front':'Blender -Y / glTF +Z','integration':'3D source for the character portrait; first-person game does not render an external player body.'},indent=2)+'\n')
bpy.data.libraries.write(str(ART/'stump.blend'),{scene},fake_user=True)
print('Stump authored:',len(parts),'parts,',triangles,'triangles')

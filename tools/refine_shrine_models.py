"""Author the woodland shrine replacement in an isolated Blender scene.

Execute through Blender MCP, or Blender --background --python this_file.
Only this generator's own scene and shrine output files are replaced.
The two runtime meshes share the unchanged, embedded Kenney palette.
"""
from pathlib import Path
import hashlib
import json
import math

import bpy
import bmesh
from mathutils import Euler, Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/shrines/rework'
ART.mkdir(parents=True, exist_ok=True)
OUT = ROOT / 'assets/models/world_landmarks'
SCENE_NAME = 'IAN_SHRINES_REWORK_20261028'
previous = bpy.data.scenes.get(SCENE_NAME)
if previous:
    assert previous.get('ian_shrine_generator') == 2
    for obj in list(previous.objects):
        data = obj.data if obj.type == 'MESH' else None
        bpy.data.objects.remove(obj, do_unlink=True)
        if data and data.users == 0:
            bpy.data.meshes.remove(data)
    bpy.data.scenes.remove(previous)
for mesh in list(bpy.data.meshes):
    if mesh.name.startswith(('woodland_player_shrine_', 'woodland_building_shrine_')) and mesh.users == 0:
        bpy.data.meshes.remove(mesh)
for collection in list(bpy.data.collections):
    if collection.name.startswith('SHRINES_REWORK_') and collection.users == 0:
        bpy.data.collections.remove(collection)
scene = bpy.data.scenes.new(SCENE_NAME)
scene['ian_shrine_generator'] = 2
bpy.context.window.scene = scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1
models = bpy.data.collections.new('SHRINES_REWORK_MODEL')
scene.collection.children.link(models)
studio = bpy.data.collections.new('SHRINES_REWORK_STUDIO')
scene.collection.children.link(studio)

palette_path = ROOT / 'assets/models/buildings/Textures/colormap.png'
image = bpy.data.images.load(str(palette_path), check_existing=False)
image.name = 'Shrines_Original_Kenney_Palette'
image.pack()
material = bpy.data.materials.new('Shrines_Woodland_Colormap')
material.use_nodes = True
shader = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
shader.inputs['Roughness'].default_value = 1
shader.inputs['Metallic'].default_value = 0
tex = material.node_tree.nodes.new('ShaderNodeTexImage')
tex.image = image
tex.interpolation = 'Linear'
material.node_tree.links.new(tex.outputs['Color'], shader.inputs['Base Color'])

# Blender UVs, inside the original kit swatches. Gradients match its buildings.
WOOD = (.09375, .28, .47)
OAK = (.21875, .28, .47)
ENDGRAIN = (.34375, .31, .46)
STONE = (.59375, .035, .19)
LIGHT_STONE = (.84375, .035, .19)
IRON = (.46875, .04, .18)
GOLD = (.65625, .28, .46)
PURPLE = (.46875, .535, .705)
BLUE = (.03125, .535, .705)
RED = (.96875, .28, .46)
MOSS = (.59375, .285, .44)


class Geo:
    """One palette mesh, with editable component vertex groups."""
    def __init__(self):
        self.vertices = []
        self.faces = []
        self.uvs = []
        self.smooth = []
        self.groups = {}

    def component(self, name, vertices, faces, swatch, smooth=()):
        start = len(self.vertices)
        self.vertices.extend(vertices)
        low = min(v[2] for v in vertices)
        high = max(v[2] for v in vertices)
        for i, face in enumerate(faces):
            self.faces.append(tuple(start + k for k in face))
            self.smooth.append(i in smooth)
            self.uvs.append([(swatch[0], swatch[1] + (swatch[2] - swatch[1]) *
                              (vertices[k][2] - low) / max(high - low, 1e-6)) for k in face])
        self.groups[name] = list(range(start, len(self.vertices)))

    def lathe(self, name, profile, swatch, center=(0, 0, 0), sides=12, smooth=True, rotation=None):
        vertices = []
        for radius, height in profile:
            for i in range(sides):
                angle = math.tau * i / sides + math.pi / sides
                p = Vector((radius * math.cos(angle), radius * math.sin(angle), height))
                if rotation is not None:
                    p = rotation @ p
                vertices.append(tuple(p + Vector(center)))
        faces = [tuple(reversed(range(sides)))]
        for row in range(len(profile) - 1):
            for i in range(sides):
                k = (i + 1) % sides
                faces.append((row*sides+i, row*sides+k, (row+1)*sides+k, (row+1)*sides+i))
        faces.append(tuple((len(profile)-1)*sides+i for i in range(sides)))
        self.component(name, vertices, faces, swatch,
                       range(1, len(faces)-1) if smooth else ())

    def box(self, name, center, size, swatch, bevel=.035, rotation=(0, 0, 0)):
        x, y, z = (v*.5 for v in size)
        bevel = min(bevel, x*.4, y*.4, z*.4)
        transform = Euler(rotation).to_matrix()
        vertices = []
        for height, inset in [(-z, bevel), (-z+bevel, 0), (z-bevel, 0), (z, bevel)]:
            a, b = x-inset, y-inset
            corner = min(bevel, a*.3, b*.3)
            for vx, vy in [(-a+corner,-b),(a-corner,-b),(a,-b+corner),(a,b-corner),
                           (a-corner,b),(-a+corner,b),(-a,b-corner),(-a,-b+corner)]:
                vertices.append(tuple(transform @ Vector((vx, vy, height)) + Vector(center)))
        faces = [tuple(reversed(range(8)))]
        for row in range(3):
            for i in range(8):
                k = (i+1) % 8
                faces.append((row*8+i,row*8+k,(row+1)*8+k,(row+1)*8+i))
        faces.append(tuple(24+i for i in range(8)))
        self.component(name, vertices, faces, swatch)

    def prism(self, name, outline, center, depth, swatch, bevel=.08):
        # XZ outline with a real bevel, rather than a flat sign facing the camera.
        n = len(outline)
        vertices = []
        for y, scale in [(-depth*.5,1-bevel),(-depth*.35,1),(depth*.35,1),(depth*.5,1-bevel)]:
            vertices.extend((center[0]+x*scale,center[1]+y,center[2]+z*scale) for x,z in outline)
        faces = [tuple(reversed(range(n)))]
        for row in range(3):
            for i in range(n):
                k=(i+1)%n
                faces.append((row*n+i,row*n+k,(row+1)*n+k,(row+1)*n+i))
        faces.append(tuple(3*n+i for i in range(n)))
        self.component(name, vertices, faces, swatch)

    def ring(self, name, outline, inner_radius, center, depth, swatch):
        n = len(outline)
        vertices = []
        for y, inner in [(-depth*.5,False),(depth*.5,False),(-depth*.5,True),(depth*.5,True)]:
            for x,z in outline:
                if inner:
                    angle=math.atan2(z,x)
                    x,z=inner_radius*math.cos(angle),inner_radius*math.sin(angle)
                vertices.append((center[0]+x,center[1]+y,center[2]+z))
        faces=[]
        for i in range(n):
            k=(i+1)%n
            faces.extend([(i,k,n+k,n+i),(i,2*n+i,2*n+k,k),
                          (n+k,3*n+k,3*n+i,n+i),(2*n+k,2*n+i,3*n+i,3*n+k)])
        self.component(name,vertices,faces,swatch)

    def gem(self, name, center, radius, height, swatch, lean=(0,0,0)):
        self.lathe(name,[(radius*.35,0),(radius,height*.19),(radius*.94,height*.66),
                         (radius*.13,height)],swatch,center,6,False,Euler(lean).to_matrix())

    def object(self,name):
        mesh=bpy.data.meshes.new(name+'_Mesh')
        mesh.from_pydata(self.vertices,[],self.faces)
        mesh.update()
        uv=mesh.uv_layers.new(name='UVMap')
        for polygon,coords,smooth in zip(mesh.polygons,self.uvs,self.smooth):
            polygon.use_smooth=smooth
            for loop,coord in zip(polygon.loop_indices,coords): uv.data[loop].uv=coord
        bm=bmesh.new();bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        bm.to_mesh(mesh);bm.free();mesh.update()
        mesh.materials.append(material)
        obj=bpy.data.objects.new(name,mesh);models.objects.link(obj)
        for group,verts in self.groups.items():obj.vertex_groups.new(name=group).add(verts,1,'REPLACE')
        return obj


def bolt(g,name,x,y,z,r=.055):
    rotation=Matrix.Rotation(math.pi*.5,3,'X')
    g.lathe(name,[(r*.8,0),(r,.015),(r,.045),(r*.75,.058)],LIGHT_STONE,
            (x,y,z),6,False,rotation)


def base(g,kind):
    accent=PURPLE if kind=='player' else BLUE
    g.lathe('foundation_hewn_stone',[(.80,0),(.96,.075),(.96,.19),(.86,.29),(.79,.31)],
            STONE,sides=8,smooth=False)
    g.lathe('upper_stone_lip',[(.77,.29),(.84,.34),(.84,.42),(.77,.47)],LIGHT_STONE,
            sides=8,smooth=False)
    g.box('offering_step_front',(0,-.67,.21),(.84,.60,.26),STONE,.045)
    g.box('offering_step_top',(0,-.78,.36),(.69,.42,.10),LIGHT_STONE,.025)
    g.lathe('altar_wood_body',[(.49,.43),(.55,.51),(.45,.61),(.43,1.10),(.51,1.16)],
            WOOD,sides=8,smooth=False)
    g.lathe('lower_iron_band',[(.50,.49),(.55,.52),(.55,.63),(.49,.66)],IRON,sides=8,smooth=False)
    g.lathe('altar_stone_table',[(.48,1.10),(.61,1.15),(.63,1.24),(.56,1.31)],
            LIGHT_STONE,sides=10,smooth=False)
    g.lathe('offering_recess',[(.40,1.311),(.40,1.325)],IRON,sides=10,smooth=False)
    g.lathe('magic_pool',[(.33,1.326),(.33,1.345)],accent,sides=10,smooth=False)
    # Wide metal straps, not thin ornamental lines, remain legible from the path.
    for sign in (-1,1):
        g.box('altar_strap_'+str(sign),(sign*.28,-.383,.86),(.11,.075,.49),IRON,.018,
              (0,0,sign*-.13))
        bolt(g,'altar_rivet_'+str(sign),sign*.27,-.438,.90,.045)
    diamond=[(0,.20),(.17,0),(0,-.20),(-.17,0)]
    g.prism('offering_badge_rim',diamond,(0,-.455,.86),.065,GOLD)
    g.prism('offering_badge_crystal',[(x*.72,z*.72) for x,z in diamond],
            (0,-.506,.86),.075,accent)
    for i,(x,y,z,r,h) in enumerate([(-.70,-.28,.40,.14,.48),(-.53,-.42,.42,.09,.28),
                                   (.70,.30,.40,.13,.41)]):
        g.gem('ground_crystal_'+str(i),(x,y,z),r,h,accent,(0,-.15 if x<0 else .18,0))
    for i,(x,y) in enumerate([(-.62,.48),(.65,-.36),(-.40,-.66)]):
        g.box('moss_pad_'+str(i),(x,y,.43),(.24,.16,.065),MOSS,.028,(0,0,i*.72))


def player_body(g):
    base(g,'player')
    # The two shoulders are deliberately offset; it is an old woodland altar.
    for sign in (-1,1):
        g.box('arch_stone_foot_'+str(sign),(sign*.65,.13,.62),(.42,.52,.48),STONE,.065)
        g.box('arch_upright_'+str(sign),(sign*.67,.15,1.39),(.33,.39,1.36),LIGHT_STONE,.05,
              (0,sign*-.075,0))
        g.box('arch_bronze_collar_'+str(sign),(sign*.68,.15,1.18),(.39,.44,.14),GOLD,.025)
        # True wedge-shaped voussoirs: separated joints avoid overlapping,
        # coplanar cube faces and their flickering checkerboard at the crown.
        for i in range(4):
            a=math.radians(i*20+.6)
            b=math.radians((i+1)*20-.6)
            outline=[(sign*r*math.cos(t),r*math.sin(t))
                     for r,t in [(.82,a),(.82,b),(.55,b),(.55,a)]]
            cx=sum(p[0] for p in outline)/4
            cz=sum(p[1] for p in outline)/4
            g.prism('arch_crown_'+str(sign)+'_'+str(i),
                    [(x-cx,z-cz) for x,z in outline],(cx,.15,1.92+cz),.40,LIGHT_STONE,.10)
        # Purple hanging tabs make the shrine readable from behind as well.
        outline=[(-.11,.24),(.11,.24),(.11,-.19),(0,-.12),(-.11,-.19)]
        g.prism('votive_tab_'+str(sign),outline,(sign*.71,-.095,1.60),.042,PURPLE)
        bolt(g,'tab_pin_'+str(sign),sign*.71,-.135,1.76,.045)
    g.gem('crown_amethyst',(0,.15,2.59),.115,.21,PURPLE)


def heart(g):
    outline=[]
    for i in range(24):
        t=math.tau*i/24
        outline.append((.029*16*math.sin(t)**3,
                        .029*(13*math.cos(t)-5*math.cos(2*t)-2*math.cos(3*t)-math.cos(4*t))))
    outline.reverse()
    g.prism('heart_bronze_setting',[(x*1.09,z*1.09) for x,z in outline],(0,-.30,2.07),.20,GOLD,.09)
    g.prism('heart_faceted_ruby',outline,(0,-.30,2.07),.31,RED,.16)
    # Small diamond below the heart reinforces the magical suspended centerpiece.
    g.gem('heart_spark',(0,-.30,1.43),.085,.19,PURPLE)


def building_body(g):
    base(g,'building')
    for sign in (-1,1):
        g.box('timber_stone_boot_'+str(sign),(sign*.65,.14,.62),(.43,.50,.41),STONE,.05)
        g.box('timber_arch_post_'+str(sign),(sign*.67,.14,1.51),(.34,.38,1.98),WOOD,.045,
              (0,sign*-.045,0))
        for z in (.76,1.45,2.33):
            g.box('post_iron_wrap_'+str(sign)+'_'+str(z),(sign*.67,.14,z),(.40,.44,.14),IRON,.02)
            bolt(g,'post_bolt_'+str(sign)+'_'+str(z),sign*.67,-.111,z,.058)
        g.box('oak_top_cap_'+str(sign),(sign*.69,.14,2.56),(.43,.45,.14),ENDGRAIN,.035)
    g.box('timber_crossbeam',(0,.14,2.55),(1.68,.39,.26),OAK,.05)
    g.box('crossbeam_iron_center',(0,.14,2.55),(.25,.43,.28),IRON,.025)
    bolt(g,'crossbeam_rivet',0,-.11,2.55,.06)
    g.gem('crossbeam_sapphire',(0,.14,2.67),.13,.13,BLUE)
    # Diagonal shoulders make a silhouette distinct from the player's stone arch.
    for sign in (-1,1):
        g.box('timber_diagonal_'+str(sign),(sign*.47,.25,2.25),(.15,.18,.57),OAK,.022,
              (0,sign*.67,0))
    # A little forged anvil lives in the offering dish; no detached tiny tools.
    anvil=[(-.27,.07),(-.22,.12),(.18,.12),(.31,.04),(.15,.01),(.11,-.10),
           (.18,-.15),(-.18,-.15),(-.12,-.09),(-.14,.01),(-.27,.01)]
    g.prism('anvil_offering',anvil,(0,-.05,1.50),.25,IRON,.055)
    g.box('anvil_top_edge',(-.025,-.05,1.616),(.39,.26,.055),LIGHT_STONE,.016)


def gear(g):
    outline=[]
    for i in range(40):
        angle=math.tau*i/40
        radius=.47 if i%5 in (1,2,3) else .385
        outline.append((radius*math.cos(angle),radius*math.sin(angle)))
    g.ring('eight_tooth_bronze_gear',outline,.23,(0,-.31,2.04),.24,GOLD)
    ring=[(.275*math.cos(math.tau*i/16),.275*math.sin(math.tau*i/16)) for i in range(16)]
    g.ring('gear_steel_hub',ring,.20,(0,-.31,2.04),.285,LIGHT_STONE)
    # A real double-pyramid gem, visible and faceted from both sides.
    vertices=[(0,-.535,2.04),(0,-.085,2.04),(-.183,-.31,2.04),
              (0,-.31,2.235),(.183,-.31,2.04),(0,-.31,1.845)]
    faces=[]
    for i,k in [(2,3),(3,4),(4,5),(5,2)]:faces.extend([(0,i,k),(1,k,i)])
    g.component('gear_sapphire',vertices,faces,BLUE)


manifest={'version':2,'style':'hewn stone / banded oak / bronze / faceted crystals',
          'palette_sha256':hashlib.sha256(palette_path.read_bytes()).hexdigest(),
          'license':'Original geometry; existing Kenney CC0 colormap', 'models':{}}
roots=[]
for kind,body_builder,glyph_builder in [('player',player_body,heart),('building',building_body,gear)]:
    root=bpy.data.objects.new('woodland_'+kind+'_shrine_root',None);models.objects.link(root)
    g=Geo();body_builder(g);body=g.object('woodland_'+kind+'_shrine_body');body.parent=root
    g=Geo();glyph_builder(g);glyph=g.object('woodland_'+kind+'_shrine_blessing');glyph.parent=root
    # Bake the runtime height, avoiding a different normalization per shrine.
    top=max(v.co.z for obj in (body,glyph) for v in obj.data.vertices)
    scale=2.8/top
    for obj in (body,glyph):
        for v in obj.data.vertices:v.co*=scale
        obj.data.update()
    bpy.ops.object.select_all(action='DESELECT')
    for obj in (root,body,glyph):obj.select_set(True)
    bpy.context.view_layer.objects.active=body
    bpy.ops.export_scene.gltf(filepath=str(OUT/(kind+'_shrine.glb')),export_format='GLB',
                              use_selection=True,use_active_scene=True,export_yup=True)
    coords=[v.co for obj in (body,glyph) for v in obj.data.vertices]
    counts={}
    for obj in (body,glyph):
        obj.data.calc_loop_triangles()
        counts[obj.name]={'triangles':len(obj.data.loop_triangles),
                          'editable_components':len(obj.vertex_groups)}
    manifest['models'][kind]={'glb':'assets/models/world_landmarks/'+kind+'_shrine.glb',
        'bounds_glb':{'min':[min(v[i] for v in coords) for i in (0,2,1)],
                      'max':[max(v[i] for v in coords) for i in (0,2,1)]},
        'pivot_glb':[0,0,0], 'runtime_height':2.8,'meshes':counts,'materials':1,
        'collision':'Unchanged game-provided centered 2x2x2.8m blocker; visible-mesh hover',
        'animation':'Static geometry; consumed tint and terrain alignment remain in game renderer',
        'integration':'Replaces the existing two shrine GLBs; no gameplay or renderer changes'}
    root.location.x=-1.28 if kind=='player' else 1.28
    roots.append(root)

world=bpy.data.worlds.new('Woodland_Shrine_Studio');world.use_nodes=True;scene.world=world
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs[0].default_value=(.05,.065,.08,1);bg.inputs[1].default_value=.6
for name,location,power,size in [('Key',(-3,-5,7),800,5),('Fill',(4,-1,5),480,4),('Rim',(1,4,6),700,4)]:
    light=bpy.data.lights.new('Woodland_Shrines_'+name,'AREA');light.energy=power;light.shape='DISK';light.size=size
    obj=bpy.data.objects.new(light.name,light);studio.objects.link(obj);obj.location=location
    obj.rotation_euler=(Vector((0,0,1.2))-obj.location).to_track_quat('-Z','Y').to_euler()
camera=bpy.data.objects.new('Woodland_Shrines_Camera',bpy.data.cameras.new('Woodland_Shrines_Camera'))
studio.objects.link(camera);scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=5.6
camera.location=(3,-8,4.3)
camera.rotation_euler=(Vector((0,0,1.3))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.view_settings.view_transform='Standard'
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ART/'preview.png')
try:scene.render.engine='BLENDER_EEVEE'
except TypeError:scene.render.engine='BLENDER_EEVEE_NEXT'
bpy.context.view_layer.update()
bpy.data.libraries.write(str(ART/'shrines.blend'),{scene},fake_user=True)
(ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
print('Authored woodland shrines in separate scene; original user scene preserved.')

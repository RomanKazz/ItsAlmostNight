from pathlib import Path
import bpy,math,json
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/stump'
bpy.ops.wm.open_mainfile(filepath=str(ART/'stump.blend'))
scene=next(s for s in bpy.data.scenes if s.name.startswith('STUMP Character'));bpy.context.window.scene=scene
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.shading.type='MATERIAL';r=area.spaces.active.region_3d
            r.view_location=(0,0,.85);r.view_distance=3.3;r.view_rotation=Vector((.5,-1,.35)).to_track_quat('Z','Y')
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'stump.blend'))
studio=bpy.data.scenes.new('Stump clean GLB review');bpy.context.window.scene=studio
studio.render.engine='BLENDER_EEVEE';studio.render.resolution_x=512;studio.render.resolution_y=512;studio.render.resolution_percentage=100
studio.render.film_transparent=True;studio.render.image_settings.file_format='PNG';studio.render.image_settings.color_mode='RGBA';studio.view_settings.view_transform='Standard'
world=bpy.data.worlds.new('Neutral portrait world');world.use_nodes=True;studio.world=world
next(n for n in world.node_tree.nodes if n.type=='BACKGROUND').inputs['Strength'].default_value=.7
camdata=bpy.data.cameras.new('Stump portrait camera');camera=bpy.data.objects.new('Stump portrait camera',camdata);studio.collection.objects.link(camera);studio.camera=camera
camdata.type='ORTHO';camdata.ortho_scale=2.35
for pos,power,size in [((-3,-4,6),300,5),((4,-2,3),100,4),((1,3,5),180,3)]:
    data=bpy.data.lights.new('Neutral softbox','AREA');data.energy=power;data.shape='DISK';data.size=size
    o=bpy.data.objects.new('Neutral softbox',data);studio.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((0,0,.85))-o.location).to_track_quat('-Z','Y').to_euler()

def render(path,direction):
    camera.location=Vector((0,0,.88))+Vector(direction)
    camera.rotation_euler=(Vector((0,0,.88))-camera.location).to_track_quat('-Z','Y').to_euler()
    studio.render.filepath=str(path);bpy.ops.render.render(write_still=True)

bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/models/characters/stump.glb'))
objects=[o for o in studio.objects if o.type=='MESH'];assert len(objects)==1
model=objects[0];assert len(model.data.materials)==1 and len(model.data.uv_layers)==1
assert all(math.isfinite(x) for v in model.data.vertices for x in v.normal)
assert all(.98<v.normal.length<1.02 for v in model.data.vertices)
for name,view in [('front',(0,-5,.15)),('side',(5,0,.15)),('back',(0,5,.15)),('top',(0,-.01,5)),('portrait',(2.4,-6,2.4))]:render(ART/(name+'.png'),view)
render(ROOT/'assets/ui/characters/stump.png',(2.4,-6,2.4))
for o in objects:bpy.data.objects.remove(o,do_unlink=True)
for name,path in [('tree','assets/models/environment/tree.glb'),('club','assets/models/tools/club.glb'),('acorn','assets/models/items/relics/acorn.glb')]:
    before=set(studio.objects);bpy.ops.import_scene.gltf(filepath=str(ROOT/path));objs=[o for o in studio.objects if o not in before]
    visible=[o for o in objs if o.type=='MESH' and not o.name.upper().startswith(('COLLISION','COLLIDER','COL_'))]
    for o in objs:
        if o.type=='MESH' and o not in visible:o.hide_render=True
    bounds=[o.matrix_world@Vector(p) for o in visible for p in o.bound_box]
    lo=Vector([min(p[i] for p in bounds) for i in range(3)]);hi=Vector([max(p[i] for p in bounds) for i in range(3)])
    center=(lo+hi)/2;scale=1.7/max(hi-lo)
    for o in visible:
        if o.type!='MESH':continue
        transform=o.matrix_world.copy()
        for v in o.data.vertices:v.co=(transform@v.co-center)*scale+Vector((0,0,.88))
        o.matrix_world.identity()
    render(ART/('reference_'+name+'.png'),(2.4,-6,2.4))
    for o in objs:bpy.data.objects.remove(o,do_unlink=True)
(ART/'validation.json').write_text(json.dumps({'one_mesh':True,'one_material':True,'single_uv':True,'unit_finite_normals':True,'clean_import':True,'five_view_review':True,'references':['tree','club','acorn'],'runtime_usage':'Portrait source; no external player body in this first-person game.'},indent=2)+'\n')
print('Stump GLB validated and portrait rendered from the clean runtime import.')

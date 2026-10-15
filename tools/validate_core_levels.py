"""Clean GLB import, geometry/palette checks and complete crystal orbit sweep.

Blender --background --factory-startup --python-exit-code 1 --python tools/validate_core_levels.py
Also callable through Blender MCP; does not delete other scenes.
"""
from pathlib import Path
import hashlib,json,math,struct
import bpy,bmesh
from mathutils import Matrix,Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/core_levels'
manifest=json.loads((ART/'manifest.json').read_text())
data=(ROOT/manifest['glb']).read_bytes();size=struct.unpack_from('<I',data,12)[0]
doc=json.loads(data[20:20+size]);binary=data[28+size:]
assert len(doc['meshes'])==13 and len(doc['materials'])==1
assert [n['mesh'] for n in doc['nodes'] if 'mesh' in n]==list(range(13)), 'raylib node order must match runtime mesh order'
assert not doc.get('extensionsRequired')
assert all('uri' not in b for b in doc['buffers'])
assert all('uri' not in image for image in doc['images'])
view=doc['bufferViews'][doc['images'][0]['bufferView']]
png=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
assert hashlib.sha256(png).hexdigest()==manifest['palette_sha256']
assert hashlib.sha256((ROOT/'assets/models/buildings/core.glb').read_bytes()).hexdigest()==manifest['source_sha256']

scene=bpy.data.scenes.new('CORE_CLEAN_VALIDATION');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(ROOT/manifest['glb']))
# Node names can acquire suffixes in the live file. Match the 13 imported
# meshes by raw mesh name (unique across the exported file) through nodes.
node_by_mesh={n['mesh']:n['name'] for n in doc['nodes'] if 'mesh' in n}
objects={}
for index,raw in enumerate(doc['meshes']):
    matches=[o for o in scene.objects if o.type=='MESH' and o.name.split('.')[0]==node_by_mesh[index]]
    assert len(matches)==1,(index,node_by_mesh[index])
    objects[index]=matches[0]

checks=[]
for index,obj in objects.items():
    mesh=obj.data
    finite=all(math.isfinite(v) for vertex in mesh.vertices for v in vertex.co)
    uv=mesh.uv_layers.active
    valid_uv=uv is not None and all(math.isfinite(v) and 0<=v<=1 for loop in uv.data for v in loop.uv)
    bm=bmesh.new();bm.from_mesh(mesh)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    manifold=all(edge.is_manifold for edge in bm.edges)
    positive=bm.calc_volume(signed=True)>0
    winding=all(edge.is_contiguous for edge in bm.edges)
    bm.free()
    # Check exported normals, not recalculated vertex normals after import.
    primitive=doc['meshes'][index]['primitives'][0]
    accessor=doc['accessors'][primitive['attributes']['NORMAL']]
    view=doc['bufferViews'][accessor['bufferView']]
    start=view.get('byteOffset',0)+accessor.get('byteOffset',0);stride=view.get('byteStride',12)
    normals=[struct.unpack_from('<3f',binary,start+i*stride) for i in range(accessor['count'])]
    unit=all(all(math.isfinite(v) for v in n) and abs(sum(v*v for v in n)-1)<1e-4 for n in normals)
    if index>=6:
        # Smooth radial normals on the outer vertical shoe, flat cap normals
        # separately. This specifically guards the earlier dark-cap artifact.
        a=doc['accessors'][primitive['attributes']['POSITION']];v=doc['bufferViews'][a['bufferView']]
        base=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',12)
        positions=[struct.unpack_from('<3f',binary,base+i*stride) for i in range(a['count'])]
        sides=[(p,n) for p,n in zip(positions,normals) if abs(math.hypot(p[0],p[2])-.5)<1e-5 and abs(n[1])<1e-5]
        assert len(sides)>=32 and all((p[0]*n[0]+p[2]*n[2])/.5>.999 for p,n in sides)
        assert any(abs(n[1])>.999 for n in normals), 'flat cap normals must remain hard'
    mesh.calc_loop_triangles()
    checks.append({'mesh':node_by_mesh[index],'finite':finite,'valid_uv':valid_uv,'manifold':manifold,'outward_winding':winding and positive,'unit_normals':unit,'triangles':len(mesh.loop_triangles)})
    assert finite and valid_uv and manifold and positive and winding and unit,checks[-1]

def vertices(index,pose=Matrix.Identity(4)):
    obj=objects[index]
    return [pose@obj.matrix_world@v.co for v in obj.data.vertices]

def tree(index,pose=Matrix.Identity(4)):
    obj=objects[index];obj.data.calc_loop_triangles()
    return BVHTree.FromPolygons(vertices(index,pose),[tuple(t.vertices) for t in obj.data.loop_triangles],all_triangles=True)

collisions=[];frames=0
for level in range(1,9):
    p=level-1;lift=p*.035;growth=1+p*.045
    count=4 if level<3 else 6 if level<6 else 8
    pedestal=tree(level+4)
    pedestal_vertices=vertices(level+4)
    assert all(-.5001<=v.x<=.5001 and -.5001<=v.y<=.5001 and v.z>=-.00234
               for v in pedestal_vertices), ('pedestal footprint',level)
    if level>=2:
        # The 16-sided shoe is narrower between vertices than its circumradius.
        # Keep low column/brace feet within every actual edge, not only r<=.5.
        shoe_edge=.5*math.cos(math.pi/16)
        assert all(v.x*math.cos(math.pi/16+i*math.tau/16)+
                   v.y*math.sin(math.pi/16+i*math.tau/16)<=shoe_edge+1e-6
                   for v in pedestal_vertices if v.z<=.055001 for i in range(16)), ('foot projects past shoe',level)
        points=KDTree(len(pedestal_vertices))
        for index,v in enumerate(pedestal_vertices):points.insert(v,index)
        points.balance()
        assert all(points.find(Vector((-v.y,v.x,v.z)))[2]<1e-6
                   for v in pedestal_vertices), ('fourfold symmetry',level)
        groups=manifest['levels'][str(level)]['components']
        assert all(f'gold_armored_support_{i}' in groups for i in range(1,5))
        if level>=4:assert all(f'corner_reactor_pylon_{i}' in groups for i in range(1,5))
        if level>=6:assert all(f'crown_crystal_clamp_{i}' in groups for i in range(1,5))
        if level==8:assert 'master_crown' in groups
    center_pose=Matrix.Translation((0,0,.267762+lift))@Matrix.Diagonal((1+(growth-1)*.5,1+(growth-1)*.5,growth,1))@Matrix.Translation((0,0,-.267762))
    center=tree(0,center_pose)
    assert not pedestal.overlap(center), ('central crystal intersects pedestal',level)
    for degree in range(360):
        # Sample a complete orbit and bob cycle with phased satellites.
        seconds=degree*math.pi/180/.28
        orbit=[]
        rings=[(count,.29+p*.008,.491512+lift,1+p*.025,.28,level>=3)]
        if level>=7:rings.append((6 if level==8 else 4,.26,.94+(level-7)*.04,.48,-.20,False))
        for ring_count,radius,height,base_size,speed,stagger in rings:
            for i in range(ring_count):
                phase=math.tau*i/ring_count;angle=seconds*speed+phase
                bob=.015*math.sin(seconds*math.tau/6+phase)
                small=stagger and i%2
                size=base_size*(.72 if small else 1)
                pose=Matrix.Translation((math.sin(angle)*radius,math.cos(angle)*radius,height+bob+(.04 if small else 0)))@Matrix.Rotation(angle,4,'Z')@Matrix.Diagonal((size,size,size,1))@Matrix.Translation((0,-.25,-.441512))
                moving=tree(1,pose)
                if pedestal.overlap(moving) or center.overlap(moving):collisions.append((level,degree,i,'body'))
                if any(previous.overlap(moving) for previous in orbit):collisions.append((level,degree,i,'satellite'))
                orbit.append(moving)
                top=.267762+(.815262-.267762)*growth+lift
                assert all(-.5001<=v.x<=.5001 and -.5001<=v.y<=.5001 and -.00234<=v.z<=top+.0001 for v in vertices(1,pose))
        frames+=1
assert not collisions,collisions[:12]
result={'geometry':checks,'palette_byte_identical':True,'original_preserved':True,'column_feet_inside_shoe':True,'pedestal_bounds_and_symmetry':True,'central_crystal_clearance':True,'orbit_frames':frames,'orbit_collisions':collisions,'triangles_per_level':[checks[0]['triangles']+checks[level+4]['triangles']+(4 if level<3 else 6 if level<6 else 8 if level==6 else 12 if level==7 else 14)*checks[1]['triangles'] for level in range(1,9)],'loader_check':'C++ graphics smoke test (separate)','native_playtest':False}
(ART/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))

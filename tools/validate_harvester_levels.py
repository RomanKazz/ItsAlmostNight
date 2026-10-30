"""Raw GLB, source, fresh import and full mechanical sweep for 24 harvester variants."""
from pathlib import Path
import bpy,bmesh,json,struct,hashlib,math,re
from mathutils import Vector,Matrix,Quaternion
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/harvester_levels'
manifest=json.loads((ART/'manifest.json').read_text());PNG=(ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()
def close(a,b):assert (Vector(a)-Vector(b)).length<1e-5,(tuple(a),b)
def group_ids(o,name):
    g=o.vertex_groups[name].index
    return {v.index for v in o.data.vertices if any(w.group==g for w in v.groups)}
def tree(o,only=None,exclude=()):
    o.data.calc_loop_triangles();chosen=group_ids(o,only) if only else None
    omitted=set().union(*(group_ids(o,n) for n in exclude)) if exclude else set()
    faces=[tuple(t.vertices) for t in o.data.loop_triangles if (chosen is None or all(i in chosen for i in t.vertices)) and not all(i in omitted for i in t.vertices)]
    return BVHTree.FromPolygons([o.matrix_world@v.co for v in o.data.vertices],faces,all_triangles=True)
def matrix(n):
    if 'matrix' in n:return Matrix([n['matrix'][i::4] for i in range(4)])
    x,y,z,w=n.get('rotation',[0,0,0,1]);return Matrix.LocRotScale(Vector(n.get('translation',[0,0,0])),Quaternion((w,x,y,z)),Vector(n.get('scale',[1,1,1])))
reports=[]
for row in manifest['levels']:
    path=ROOT/row['glb'];raw=path.read_bytes();assert hashlib.sha256(raw).hexdigest()==row['sha256']
    assert struct.unpack_from('<III',raw)==(0x46546c67,2,len(raw))
    size,kind=struct.unpack_from('<II',raw,12);assert kind==0x4e4f534a;doc=json.loads(raw[20:20+size])
    binary_len,kind=struct.unpack_from('<II',raw,20+size);assert kind==0x004e4942;binary=raw[28+size:28+size+binary_len]
    assert not doc.get('extensionsRequired') and not doc.get('animations') and not doc.get('skins') and not doc.get('cameras')
    assert len(doc['images'])==1 and 'uri' not in doc['images'][0] and all('uri' not in b for b in doc['buffers'])
    v=doc['bufferViews'][doc['images'][0]['bufferView']];off=v.get('byteOffset',0);assert binary[off:off+v['byteLength']]==PNG
    def values(index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']];fmt={5126:'f',5125:'I',5123:'H',5121:'B'}[a['componentType']]
        width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];form='<'+fmt*width;start=v.get('byteOffset',0)+a.get('byteOffset',0)
        stride=v.get('byteStride',struct.calcsize(form));return [struct.unpack_from(form,binary,start+i*stride) for i in range(a['count'])]
    nodes=doc['nodes'];by={n['name']:i for i,n in enumerate(nodes)};world={};parents={}
    def visit(i,parent=None):
        parents[i]=parent;world[i]=(world[parent] if parent is not None else Matrix.Identity(4))@matrix(nodes[i])
        for c in nodes[i].get('children',[]):visit(c,i)
    assert doc['scenes'][0]['nodes']==[by['asset_root']];visit(by['asset_root']);assert matrix(nodes[by['asset_root']])==Matrix.Identity(4)
    expected={'asset_root','base_mesh','module_mesh','resource_mesh','COL_BOX_BODY','socket_base','socket_resource_mount','socket_output','socket_ui_top'}
    moving_name=None
    if row['asset']=='wood_harvester':expected|={'saw_pivot','saw_mesh','socket_effect_sawdust'};moving_name='saw_mesh'
    elif row['asset']=='stone_harvester':expected|={'drill_pivot','drill_mesh','socket_effect_dust'};moving_name='drill_mesh'
    else:expected|={'socket_effect_extraction'}
    assert set(by)==expected and len(nodes)==len(by)
    points=[];tris={};materials=set();mesh_order={};colpoints=[];primitive_index=0
    for i,n in enumerate(nodes):
        assert re.fullmatch('[A-Za-z][A-Za-z0-9_]*',n['name'])
        close(n.get('scale',[1,1,1]),(1,1,1));close(n.get('rotation',[0,0,0,1]),(0,0,0,1))
        if i!=by['asset_root']:
            wanted='saw_pivot' if n['name']=='saw_mesh' else 'drill_pivot' if n['name']=='drill_mesh' else 'asset_root'
            assert parents[i]==by[wanted]
        if 'mesh' not in n:continue
        primitives=doc['meshes'][n['mesh']]['primitives'];assert len(primitives)==1
        p=primitives[0];assert p.get('mode',4)==4;attr=p['attributes'];pos=values(attr['POSITION']);normals=values(attr['NORMAL']);ind=[v[0] for v in values(p['indices'])]
        assert all(all(math.isfinite(x) for x in v) and abs(Vector(v).length-1)<1e-4 for v in normals)
        seen=set()
        for j in range(0,len(ind),3):
            ids=ind[j:j+3];v=[Vector(pos[k]) for k in ids];normal=(v[1]-v[0]).cross(v[2]-v[0]);assert normal.length>1e-9
            assert all(normal.dot(Vector(normals[k]))>0 for k in ids),(row['asset'],row['level'],n['name'],'winding')
            key=tuple(sorted(tuple(round(x,7) for x in q) for q in v));assert key not in seen,(row['asset'],row['level'],n['name'],'duplicate triangle');seen.add(key)
        tris[n['name']]=len(ind)//3;mesh_order[n['name']]=primitive_index;primitive_index+=1
        if n['name'].startswith('COL_'):
            assert len(set(pos))==8 and len(ind)==36;colpoints+=[world[i]@Vector(v) for v in pos];continue
        materials.add(p['material']);points+=[world[i]@Vector(v) for v in pos];assert 'TEXCOORD_0' in attr
        for u,v in values(attr['TEXCOORD_0']):
            assert min(abs(u-c) for c in (.09375,.15625,.21875,.34375,.46875,.53125,.59375,.65625,.84375,.96875))<1e-5
            assert .02-1e-5<=1-v<=.73+1e-5
    assert len(materials)==1 and len(doc['materials'])==2
    for m in materials:
        p=doc['materials'][m]['pbrMetallicRoughness'];assert p.get('metallicFactor',1)==0 and p.get('roughnessFactor',1)==1 and 'baseColorTexture' in p
    lo=[min(p[i] for p in points) for i in range(3)];hi=[max(p[i] for p in points) for i in range(3)]
    close(lo,row['visible_bounds_glb']['min']);close(hi,row['visible_bounds_glb']['max'])
    close([min(p[i] for p in colpoints) for i in range(3)],(-.48,0,-.48));colhi=(.48,.92 if row['asset']=='stone_harvester' else .90,.48)
    close([max(p[i] for p in colpoints) for i in range(3)],colhi)
    assert sum(v for n,v in tris.items() if not n.startswith('COL_'))==row['triangles']
    assert all(-.480001<=p.x<=.480001 and -.480001<=p.z<=.480001 and 0<=p.y<=colhi[1] for p in points)
    assert world[by['socket_ui_top']].translation.y-hi[1]>.089
    row['triangles_by_mesh']=tris;row['export_primitive_indices_in_node_order']=mesh_order
    row['render_indices_after_collision_removal']={n:j for j,n in enumerate(n for n in mesh_order if not n.startswith('COL_'))}
    row['transforms_glb']={n['name']:{'parent':nodes[parents[i]]['name'] if parents[i] is not None else None,'local_translation':n.get('translation',[0,0,0]),'local_rotation_xyzw':n.get('rotation',[0,0,0,1]),'local_scale':n.get('scale',[1,1,1]),'position_relative_to_asset_root':list(world[i].translation)} for i,n in enumerate(nodes)}
    original=(ROOT/f'assets/models/buildings/{row["asset"]}.glb').read_bytes()
    original_doc=json.loads(original[20:20+struct.unpack_from('<I',original,12)[0]])
    for node in original_doc['nodes']:
        t=row['transforms_glb'][node['name']]
        assert t['local_translation']==node.get('translation',[0,0,0])
        assert t['local_rotation_xyzw']==node.get('rotation',[0,0,0,1])
        assert t['local_scale']==node.get('scale',[1,1,1])
    row['collider_bounds_glb']={'min':[-.48,0,-.48],'max':list(colhi)}
    row['motion']={'kind':'static'} if not moving_name else {'pivot':'saw_pivot' if moving_name=='saw_mesh' else 'drill_pivot','axis_glb':[0,0,-1] if moving_name=='saw_mesh' else [0,1,0],'range_degrees':[0,360],'verified_poses':360,'recoil_m':0,'exception':'original axle inside bearing' if moving_name=='saw_mesh' else 'original core inside spindle bearing'}
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/row['blend']));scene=bpy.context.scene;bpy.context.view_layer.update()
    source=[o for o in scene.objects if o.type=='MESH' and not o.name.startswith('COL_')]
    assert len(source)==row['visible_meshes'];row['editable_vertex_groups']={o.name:len(o.vertex_groups) for o in source}
    for o in source:
        close(o.scale,(1,1,1));close(o.rotation_euler,(0,0,0))
        im=next(n.image for n in o.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE');assert im.packed_file and bytes(im.packed_file.data)==PNG
    base=scene.objects['base_mesh'];module=scene.objects['module_mesh'];resource=scene.objects['resource_mesh']
    # New visible replacement bolt faces extend past tier-8 armor; lower tiers retain original bolts.
    exposed=0
    for i in range(4):
        radial=Vector((math.cos(i*math.pi/2),math.sin(i*math.pi/2),0))
        group=('upg_master_bolt_%02d'%i) if row['level']==8 else ('harvester_bolt_%02d'%i)
        pts=[base.data.vertices[j].co for j in group_ids(base,group)];front=max(p.dot(radial) for p in pts)
        assert front>(.475 if row['level']==8 else .455)
        exposed+=sum(abs(p.dot(radial)-front)<1e-5 for p in pts)
    assert exposed==32
    poses=0
    if moving_name:
        rotor=scene.objects[moving_name];pivot=rotor.parent
        fixed=[('base',tree(base)),('module',tree(module)),('resource',tree(resource))]
        for angle in range(360):
            if moving_name=='saw_mesh':pivot.rotation_euler.y=math.radians(angle);moving=tree(rotor,exclude=('wood_saw_axle',))
            else:pivot.rotation_euler.z=math.radians(angle);moving=tree(rotor,exclude=('stone_auger_core',))
            bpy.context.view_layer.update()
            # Rebuild after the depsgraph has evaluated the new pose.
            moving=tree(rotor,exclude=('wood_saw_axle',) if moving_name=='saw_mesh' else ('stone_auger_core',))
            for name,t in fixed:assert not moving.overlap(t),(row['asset'],row['level'],angle,name,'rotor collision')
            if moving_name=='drill_mesh':
                core=tree(rotor,only='stone_auger_core')
                assert not core.overlap(tree(module,exclude=('stone_spindle_bearing',))),(row['level'],angle,'core contact outside bearing')
            for v in rotor.data.vertices:
                p=rotor.matrix_world@v.co;assert -.48<p.x<.48 and -.48<p.y<.48 and 0<p.z<colhi[1]
            poses+=1
        pivot.rotation_euler=(0,0,0);bpy.context.view_layer.update()
    elif row['level']>=3:
        crystals=tree(resource)
        for g in module.vertex_groups:
            if g.name.startswith(('upg_gold_prong','upg_crown_arm','upg_extraction_crown','upg_crown_orange','upg_capacitor')):
                assert not tree(module,only=g.name).overlap(crystals),(row['level'],g.name,'crystal collision')
    # Fresh importer checks root neutral axes, packed texture and closed visible/collision meshes.
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(path));bpy.context.view_layer.update()
    assert {o.name for o in bpy.context.scene.objects}==expected
    for o in bpy.context.scene.objects:
        if o.type!='MESH':continue
        bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
        assert all(e.is_manifold for e in bm.edges),(row['asset'],row['level'],o.name,'non-manifold after seam weld')
        assert bm.calc_volume(signed=True)>0;bm.free()
    reports.append({'asset':row['asset'],'level':row['level'],'raw_glb':True,'editable_source':True,'fresh_import':True,'normals_uv_manifold':True,'embedded_palette_identical':True,'spin_poses':poses,'exposed_bolt_face_vertices':exposed})
    print('HARVESTER_TIER_VALID',row['asset'],row['level'],poses,flush=True)
for f,h in manifest['original_sha256'].items():assert hashlib.sha256((ROOT/f).read_bytes()).hexdigest()==h
manifest['validation']={'all_variants':len(reports),'total_spin_poses':sum(r['spin_poses'] for r in reports),'original_models_preserved':True,'original_node_transforms_preserved':True,'report':'art/models/harvester_levels/validation.json'}
(ART/'validation.json').write_text(json.dumps(reports,indent=2)+'\n');(ART/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print('ALL_HARVESTER_TIERS_VALIDATED',len(reports),manifest['validation']['total_spin_poses'],flush=True)

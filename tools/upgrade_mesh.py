"""Small stdlib GLB baker shared by the building upgrade authoring tools."""
import json
import math
import struct

class GlbAsset:
    def __init__(self, path):
        self.path = path
        data = path.read_bytes()
        size = struct.unpack_from('<I', data, 12)[0]
        self.doc = json.loads(data[20:20+size])
        binary_size = struct.unpack_from('<I', data, 20+size)[0]
        self.binary = bytearray(data[28+size:28+size+binary_size])
        doc = self.doc
        # Generated geometry is appended after all authored data. Recover
        # the original asset, so running an authoring tool is idempotent.
        first = next((i for i,m in enumerate(doc['meshes'])
                      if m.get('name','').startswith('upgrade_')), None)
        if first is not None:
            primitive = doc['meshes'][first]['primitives'][0]
            accessor = primitive['attributes']['POSITION']
            view = doc['accessors'][accessor]['bufferView']
            offset = doc['bufferViews'][view].get('byteOffset',0)
            node = next(i for i,n in enumerate(doc['nodes']) if n.get('mesh') == first)
            doc['meshes'] = doc['meshes'][:first]
            doc['nodes'] = doc['nodes'][:node]
            doc['accessors'] = doc['accessors'][:accessor]
            doc['bufferViews'] = doc['bufferViews'][:view]
            doc['materials'] = doc['materials'][:primitive['material']]
            for scene in doc['scenes']:
                scene['nodes'] = [n for n in scene['nodes'] if n < node]
            self.binary = self.binary[:offset]
        else:
            self.binary = self.binary[:doc['buffers'][0]['byteLength']]
        self.upgrade_material = len(doc['materials'])
        doc['materials'].append({'name':'upgrade_shell','pbrMetallicRoughness':{
            'baseColorFactor':[1,1,1,1],'metallicFactor':0,'roughnessFactor':.4}})

    def write(self):
        self.doc['buffers'][0]['byteLength'] = len(self.binary)
        encoded = json.dumps(self.doc,separators=(',',':')).encode()
        encoded += b' ' * (-len(encoded) % 4)
        binary = self.binary + b'\0' * (-len(self.binary) % 4)
        total = 12+8+len(encoded)+8+len(binary)
        self.path.write_bytes(struct.pack('<III',0x46546c67,2,total)+
            struct.pack('<II',len(encoded),0x4e4f534a)+encoded+
            struct.pack('<II',len(binary),0x004e4942)+binary)
        return total

class Geometry:
    def __init__(self):
        self.positions, self.normals, self.colors = [], [], []

    def face(self, points, color):
        a, b, c = points[:3]
        u, v = [b[i]-a[i] for i in range(3)], [c[i]-a[i] for i in range(3)]
        normal = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
        length = math.sqrt(sum(n*n for n in normal))
        if length < 1e-9:
            return
        normal = [n/length for n in normal]
        for i in range(1, len(points)-1):
            for p in (points[0], points[i], points[i+1]):
                self.positions.extend(p)
                self.normals.extend(normal)
                self.colors.extend((*color, 255))

    def prism(self, polygon, axis, lo, hi, color):
        # Polygon is counterclockwise in the other two axes. Orient every
        # face outwards independently, including mirrored side panels.
        axes = [i for i in range(3) if i != axis]
        def point(p, height):
            result = [0, 0, 0]
            result[axis] = height
            result[axes[0]], result[axes[1]] = p
            return tuple(result)
        center = [0, 0, 0]
        center[axis] = (lo+hi)/2
        for i, ax in enumerate(axes):
            center[ax] = sum(p[i] for p in polygon)/len(polygon)
        def outward(points):
            a, b, c = points[:3]
            u, v = [b[i]-a[i] for i in range(3)], [c[i]-a[i] for i in range(3)]
            n = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
            if sum(n[i]*(a[i]-center[i]) for i in range(3)) < 0:
                points = list(reversed(points))
            self.face(points, color)
        outward([point(p, lo) for p in polygon])
        outward([point(p, hi) for p in polygon])
        for i, p in enumerate(polygon):
            q = polygon[(i+1)%len(polygon)]
            outward([point(p, lo), point(q, lo), point(q, hi), point(p, hi)])

    def box(self, center, size, color, bevel=.15):
        x, y, z = center
        w, h, d = size
        b = min(w, d)*bevel
        polygon = [(x-w/2+b,z-d/2),(x+w/2-b,z-d/2),(x+w/2,z-d/2+b),
                   (x+w/2,z+d/2-b),(x+w/2-b,z+d/2),(x-w/2+b,z+d/2),
                   (x-w/2,z+d/2-b),(x-w/2,z-d/2+b)]
        self.prism(polygon, 1, y-h/2, y+h/2, color)

    def ring(self, axis, center, outer, inner, length, color, segments=12):
        axes = [i for i in range(3) if i != axis]
        def p(r, h, t):
            result = list(center)
            result[axis] += h
            result[axes[0]] += r*math.cos(t)
            result[axes[1]] += r*math.sin(t)
            return tuple(result)
        for i in range(segments):
            a, b = 2*math.pi*i/segments, 2*math.pi*(i+1)/segments
            faces = [[p(outer,-length/2,a),p(outer,length/2,a),p(outer,length/2,b),p(outer,-length/2,b)],
                     [p(inner,-length/2,b),p(inner,length/2,b),p(inner,length/2,a),p(inner,-length/2,a)],
                     [p(inner,length/2,a),p(inner,length/2,b),p(outer,length/2,b),p(outer,length/2,a)],
                     [p(inner,-length/2,b),p(inner,-length/2,a),p(outer,-length/2,a),p(outer,-length/2,b)]]
            # Axis Y reverses the cyclic orientation of the other two axes.
            for face in faces:
                self.face(face if axis == 1 else list(reversed(face)), color)

    def bounds(self):
        return ([min(self.positions[i::3]) for i in range(3)],
                [max(self.positions[i::3]) for i in range(3)])

    def smoothed_normals(self, crease_degrees=60):
        """Weld shading across triangle seams, retaining sharp/material edges.

        Deduplicate incident face normals so triangulating a quad or cap does
        not bias the average. Authored smooth rock normals already agree at
        shared positions and pass through unchanged. Baking leaves topology,
        colors, collision bounds and the number of runtime draws untouched.
        """
        groups, keys = {}, []
        for vertex in range(len(self.positions)//3):
            offset = vertex*3
            key = (tuple(round(value, 6) for value in self.positions[offset:offset+3]),
                   tuple(self.colors[vertex*4:vertex*4+4]))
            keys.append(key)
            groups.setdefault(key, set()).add(tuple(self.normals[offset:offset+3]))
        threshold = math.cos(math.radians(crease_degrees))
        result, cache = [], {}
        for vertex, key in enumerate(keys):
            normal = tuple(self.normals[vertex*3:vertex*3+3])
            cache_key = (key, normal)
            if cache_key not in cache:
                incident = sorted(groups[key])
                neighbors = [value for value in incident
                             if sum(normal[i]*value[i] for i in range(3)) >= threshold - 1e-7]
                if len(neighbors) <= 1:
                    cache[cache_key] = normal
                else:
                    average = [sum(value[i] for value in neighbors) for i in range(3)]
                    length = math.sqrt(sum(value*value for value in average))
                    cache[cache_key] = tuple(value/length for value in average) if length > 1e-9 else normal
            result.extend(cache[cache_key])
        return result

    def append(self, asset, name):
        doc, binary = asset.doc, asset.binary
        def accessor(values, count, kind, component, fmt, bounds=None):
            binary.extend(b'\0' * (-len(binary) % 4))
            offset = len(binary)
            binary.extend(struct.pack('<'+fmt*len(values), *values))
            view = len(doc['bufferViews'])
            doc['bufferViews'].append({'buffer': 0, 'byteOffset': offset,
                                      'byteLength': len(binary)-offset, 'target': 34962})
            entry = {'bufferView': view, 'componentType': component, 'count': count, 'type': kind}
            if bounds:
                entry.update(min=bounds[0], max=bounds[1])
            if component == 5121:
                entry['normalized'] = True
            doc['accessors'].append(entry)
            return len(doc['accessors'])-1
        count = len(self.positions)//3
        attributes = {'POSITION': accessor(self.positions,count,'VEC3',5126,'f',self.bounds()),
                      'NORMAL': accessor(self.smoothed_normals(),count,'VEC3',5126,'f'),
                      'COLOR_0': accessor(self.colors,count,'VEC4',5121,'B')}
        mesh = len(doc['meshes'])
        doc['meshes'].append({'name': name, 'primitives': [{'attributes': attributes, 'material': asset.upgrade_material}]})
        node = len(doc['nodes'])
        doc['nodes'].append({'name': name, 'mesh': mesh})
        doc['scenes'][doc.get('scene',0)]['nodes'].append(node)

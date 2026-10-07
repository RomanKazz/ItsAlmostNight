"""Bake low-poly upgrade shells into gun_turret.glb; stdlib only.

Run from the repository root. Original base, housing, texture and muzzle
nodes are preserved. Previously generated meshes are replaced, not appended.
Each level uses two vertex-colored meshes: stationary base and rotating head.
"""
import json
import math
import struct
from pathlib import Path

PATH = Path('assets/models/buildings/gun_turret.glb')
data = PATH.read_bytes()
json_size = struct.unpack_from('<I', data, 12)[0]
doc = json.loads(data[20:20 + json_size])
start = 20 + json_size
binary_size = struct.unpack_from('<I', data, start)[0]
binary = bytearray(data[start + 8:start + 8 + binary_size])
# Authored asset has three meshes (including COL_BOX), 12 accessors, 13 views.
doc['meshes'] = doc['meshes'][:3]
doc['nodes'] = doc['nodes'][:9]
doc['accessors'] = doc['accessors'][:12]
doc['bufferViews'] = doc['bufferViews'][:13]
doc['materials'] = doc['materials'][:1]
doc['scenes'][doc.get('scene', 0)]['nodes'] = [8]
binary = binary[:51804]
doc['materials'].append({'name': 'upgrade_shell', 'pbrMetallicRoughness': {
    'baseColorFactor': [1, 1, 1, 1], 'metallicFactor': 0, 'roughnessFactor': .4}})
STEEL = (150, 172, 181)
DARK = (51, 65, 72)
ORANGE = (245, 92, 12)
BRASS = (228, 165, 51)

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

    def append(self, name):
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
                      'NORMAL': accessor(self.normals,count,'VEC3',5126,'f'),
                      'COLOR_0': accessor(self.colors,count,'VEC4',5121,'B')}
        mesh = len(doc['meshes'])
        doc['meshes'].append({'name': name, 'primitives': [{'attributes': attributes, 'material': 1}]})
        node = len(doc['nodes'])
        doc['nodes'].append({'name': name, 'mesh': mesh})
        doc['scenes'][doc.get('scene',0)]['nodes'].append(node)

bounds = []
for level in range(2, 9):
    base, head = Geometry(), Geometry()
    # A tapered octagonal bearing, not a square slab underneath the body.
    base.ring(1,(0,.255,0),.335,.278,.068,STEEL)
    if level >= 4:
        for side in (-1,1):
            base.box((side*.36,.135,0),(.12,.08,.38),STEEL)
    if level >= 5:
        base.ring(1,(0,.285,0),.339,.305,.018,BRASS)
    if level >= 8:
        for side in (-1,1):
            base.box((0,.095,side*.365),(.32,.035,.11),DARK)
            base.box((0,.116,side*.365),(.12,.014,.095),BRASS)
    # Shaped cheeks follow the curved original orange housing. The front
    # slopes back towards the roof, leaving the silver muzzle face exposed.
    cheek = [(.49,-.28),(.51,.39),(.86,.43),(.98,.28),(.94,-.17),(.77,-.34)]
    for side in (-1,1):
        lo, hi = sorted((side*.418,side*.458))
        head.prism(cheek,0,lo,hi,STEEL)
        if level >= 3:
            panel = [(.54,-.225),(.56,.35),(.83,.37),(.925,.255),(.895,-.135),(.755,-.285)]
            lo, hi = sorted((side*.4585,side*.474))
            head.prism(panel,0,lo,hi,ORANGE)
            for z in (.15,.23,.31):
                vent = [(.61,z-.018),(.78,z-.018),(.795,z+.013),(.625,z+.013)]
                lo, hi = sorted((side*.475,side*.479))
                head.prism(vent,0,lo,hi,DARK)
        if level >= 5:
            # Recessed orange panel, satin metal pivot and a small brass hub.
            head.ring(0,(side*.485,.70,-.11),.093,.064,.024,DARK,10)
            disk = [( .70+.061*math.cos(i*2*math.pi/10), -.11+.061*math.sin(i*2*math.pi/10)) for i in range(10)]
            lo, hi = sorted((side*.488,side*.503))
            head.prism(disk,0,lo,hi,STEEL)
            bolt = [(.70,-.139),(.729,-.11),(.70,-.081),(.671,-.11)]
            lo, hi = sorted((side*.504,side*.51))
            head.prism(bolt,0,lo,hi,BRASS)
    if level >= 4:
        # One sculpted roof spine with sloped front/rear ends, no stacked blocks.
        roof = [(1.01,-.28),(1.01,.38),(1.065,.32),(1.10,.18),(1.10,-.10),(1.06,-.26)]
        head.prism(roof,0,-.335,.335,STEEL)
        insert = [(1.069,-.24),(1.069,.28),(1.106,.17),(1.106,-.10)]
        head.prism(insert,0,-.225,.225,ORANGE)
    if level >= 6:
        for x in (-.140388,.140388):
            # Close-fitting gunmetal barrel jacket with a single narrow lip.
            head.ring(2,(x,.685445,-.557),.128,.105,.16,DARK)
            head.ring(2,(x,.685445,-.638),.133,.102,.022,STEEL)
            head.ring(2,(x,.685445,-.651),.134,.105,.010,BRASS)
    if level >= 7:
        sight = [(1.103,-.16),(1.103,.21),(1.135,.19),(1.195,.09),(1.195,-.05),(1.16,-.12)]
        head.prism(sight,0,-.072,.072,DARK)
        crest = [(1.155,-.125),(1.201,-.055),(1.201,.085),(1.14,.185)]
        head.prism(crest,0,-.075,.075,ORANGE)
    if level >= 8:
        for side in (-1,1):
            shoulder = [(.87,-.19),(.92,.39),(1.02,.33),(1.105,.19),(1.08,-.08),(.995,-.20)]
            lo, hi = sorted((side*.33,side*.47))
            head.prism(shoulder,0,lo,hi,STEEL)
            edge = [(.998,-.205),(1.088,-.083),(1.112,.19),(1.025,.337),(.992,.345),(1.074,.182),(1.050,-.069),(.972,-.18)]
            lo, hi = sorted((side*.471,side*.48))
            head.prism(edge,0,lo,hi,BRASS)
    for part, suffix in ((base,'base'),(head,'head')):
        bounds.append(part.bounds())
        part.append(f'upgrade_l{level}_{suffix}')

doc['buffers'][0]['byteLength'] = len(binary)
encoded = json.dumps(doc,separators=(',',':')).encode()
encoded += b' ' * (-len(encoded) % 4)
binary += b'\0' * (-len(binary) % 4)
total = 12+8+len(encoded)+8+len(binary)
PATH.write_bytes(struct.pack('<III',0x46546c67,2,total)+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(binary),0x004e4942)+binary)
# Generated local bounds share the baked geometry used for drawing/aiming.
def vec(v):
    return '{'+', '.join(f'{x:.6f}F' for x in v)+'}'
header = '''#pragma once

#include <raymath.h>

#include <algorithm>
#include <array>

namespace ian::gun_turret_model_detail {

// Generated by tools/generate_gun_turret_upgrades.py. After COL_BOX removal:
// original base/housing are 0/1, followed by two baked meshes per level.
inline constexpr int MeshCount = 16;
inline constexpr std::array<BoundingBox, 14> UpgradeBounds{{
'''
header += ''.join('    {'+vec(a)+', '+vec(b)+'},\n' for a,b in bounds)
header += '''}};

template <typename Emit>
void forEachUpgradePart(int requestedLevel, Emit&& emit) {
    const int level = std::clamp(requestedLevel, 1, 8);
    if (level == 1) return;
    const int base = 2 + (level - 2)*2;
    emit(base, MatrixIdentity(), WHITE, false);
    emit(base + 1, MatrixIdentity(), WHITE, true);
}

[[nodiscard]] inline BoundingBox partBounds(int meshIndex) {
    return UpgradeBounds[static_cast<std::size_t>(meshIndex - 2)];
}

[[nodiscard]] inline float height(int requestedLevel) {
    const int level = std::clamp(requestedLevel, 1, 8);
    return level == 1 ? 1.06F : std::max(1.06F, UpgradeBounds[static_cast<std::size_t>((level - 2)*2 + 1)].max.y);
}

} // namespace ian::gun_turret_model_detail
'''
Path('src/graphics/GunTurretModel.hpp').write_text(header)
print(f'Baked seven upgrade levels: {total} bytes, two extra draws per turret.')

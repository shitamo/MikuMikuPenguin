#!/usr/bin/env python3
"""Generate the small MMD files used by the MikuMikuPenguin tests.

Third-party models and motions cannot be committed, so the tests use a tiny
model made of boxes, written from scratch by this script:

  test_model.pmx   PMX 2.0 (UTF-16): bones, IK, rotation/translation
                   inheritance (付与), vertex/UV/bone/material/group morphs,
                   rigid bodies and a joint
  test_model.pmd   PMD: a smaller model (bones, IK, a 回転影響下 bone, a morph,
                   rigid bodies and a joint)
  test_motion.vmd  bone keys with Bezier curves, morph keys and show/IK keys
  test_camera.vmd  camera keys (with a cut), light keys and self shadow keys
  tex.bmp          texture of the test model's floor material

Usage: gen_testdata.py OUTPUT_DIR

The output only depends on this script (the files are byte-for-byte the same
on every run), so the expected values in test/*_test.cpp can rely on it.
"""

import math
import os
import struct
import sys


# --------------------------------------------------------------------------
# helpers

def f32(*v):
    return struct.pack('<%df' % len(v), *v)


def u8(v):
    return struct.pack('<B', v)


def i8(v):
    return struct.pack('<b', v)


def u16(v):
    return struct.pack('<H', v)


def i32(v):
    return struct.pack('<i', v)


def u32(v):
    return struct.pack('<I', v)


def pmx_text(s):
    b = s.encode('utf-16-le')
    return i32(len(b)) + b


def sjis(s, size):
    b = s.encode('cp932')
    if len(b) > size:
        raise ValueError('%r does not fit in %d bytes' % (s, size))
    return b + b'\0' * (size - len(b))


def quat_axis_angle(axis, degrees):
    """(x, y, z, w) quaternion in MMD (left-handed) coordinates."""
    a = math.radians(degrees) / 2.0
    n = math.sqrt(sum(c * c for c in axis))
    s = math.sin(a) / n
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(a))


# --------------------------------------------------------------------------
# geometry: boxes weighted to one bone each

def box(center, size, bone):
    """24 vertices (4 per face, flat normals) and 12 triangles."""
    cx, cy, cz = center
    hx, hy, hz = size[0] / 2.0, size[1] / 2.0, size[2] / 2.0
    faces = [
        ((0, 0, -1), [(-1, -1, -1), (-1, 1, -1), (1, 1, -1), (1, -1, -1)]),  # front (MMD -Z)
        ((0, 0, 1), [(1, -1, 1), (1, 1, 1), (-1, 1, 1), (-1, -1, 1)]),      # back
        ((-1, 0, 0), [(-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (-1, -1, -1)]),  # left
        ((1, 0, 0), [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)]),      # right
        ((0, 1, 0), [(-1, 1, -1), (-1, 1, 1), (1, 1, 1), (1, 1, -1)]),      # top
        ((0, -1, 0), [(-1, -1, 1), (-1, -1, -1), (1, -1, -1), (1, -1, 1)]),  # bottom
    ]
    verts, tris = [], []
    uv = [(0, 1), (0, 0), (1, 0), (1, 1)]
    for normal, corners in faces:
        base = len(verts)
        for k, (sx, sy, sz) in enumerate(corners):
            verts.append(((cx + sx * hx, cy + sy * hy, cz + sz * hz), normal, uv[k], bone))
        # clockwise when seen from outside (Direct3D front faces)
        tris += [(base, base + 1, base + 2), (base, base + 2, base + 3)]
    return verts, tris


class Mesh:
    def __init__(self):
        self.verts = []
        self.tris = []
        self.material_tris = []  # triangles per material

    def add(self, material, center, size, bone):
        v, t = box(center, size, bone)
        base = len(self.verts)
        self.verts += v
        t = [(a + base, b + base, c + base) for a, b, c in t]
        while len(self.material_tris) <= material:
            self.material_tris.append([])
        self.material_tris[material] += t
        return base  # first vertex of the box

    def ordered_tris(self):
        return [t for tris in self.material_tris for t in tris]


# --------------------------------------------------------------------------
# PMX model

# bone flags
CONNECT_BONE = 0x0001
ROTATE = 0x0002
MOVE = 0x0004
VISIBLE = 0x0008
OPERABLE = 0x0010
IK = 0x0020
GIVE_ROT = 0x0100
GIVE_MOVE = 0x0200

# Bone list.  Indices are referenced by the C++ tests (test/mmp_test_model.h).
#   name, position, parent, level, flags, extra
PMX_BONES = [
    # 0
    ('センター', (0, 0, 0), -1, 0, ROTATE | MOVE | VISIBLE | OPERABLE, {}),
    # 1-4: arm chain driven by 腕IK
    ('体', (0, 2, 0), 0, 0, ROTATE | MOVE | VISIBLE | OPERABLE, {}),
    ('腕', (0, 6, 0), 1, 0, ROTATE | VISIBLE | OPERABLE, {}),
    ('ひじ', (0, 10, 0), 2, 0, ROTATE | VISIBLE | OPERABLE, {}),
    ('手首', (0, 14, 0), 3, 0, ROTATE | VISIBLE, {}),
    # 5
    ('腕IK', (0, 14, 0), 0, 0, ROTATE | MOVE | VISIBLE | OPERABLE | IK,
     {'ik': (4, 40, 2.0, [(3, ((-3.0, -3.0, -3.0), (3.0, 3.0, 3.0))), (2, None)])}),
    # 6-10: deformation order.  補助IK (index 7) sits under 足D (index 10),
    # which inherits the rotation of 足.  Its level (2) is above 足D's (1),
    # so 足D must be evaluated first although it has the larger index.
    ('足', (8, 10, 0), 0, 0, ROTATE | VISIBLE | OPERABLE, {}),
    ('補助IK', (8, 4, 0), 10, 2, ROTATE | MOVE | VISIBLE | IK, {'ik': (9, 20, 1.0, [(8, None)])}),
    ('補助根', (8, 10, 0), 0, 0, ROTATE | VISIBLE, {}),
    ('補助先', (8, 4, 0), 8, 0, ROTATE | VISIBLE, {}),
    ('足D', (8, 10, 0), 0, 1, ROTATE | VISIBLE | GIVE_ROT, {'give': (6, 1.0)}),
    # 11: half of 体's rotation
    ('半付与', (4, 2, 0), 0, 1, ROTATE | VISIBLE | GIVE_ROT, {'give': (1, 0.5)}),
    # 12-13: pendulum simulated by Bullet
    ('揺れ', (-6, 12, 0), 0, 0, ROTATE | VISIBLE | OPERABLE, {}),
    ('揺れ先', (-6, 6, 0), 12, 0, VISIBLE, {}),
    # 14: half of センター's translation
    ('移動付与', (-3, 2, 1), 0, 1, ROTATE | MOVE | VISIBLE | GIVE_MOVE, {'give': (0, 0.5)}),
]

# Materials: name, diffuse RGBA, edge, toon
PMX_MATERIALS = [
    ('赤', (0.9, 0.2, 0.2, 1.0), True),
    ('緑', (0.2, 0.8, 0.3, 1.0), True),
    ('青', (0.2, 0.4, 0.9, 1.0), False),
    ('床', (1.0, 1.0, 1.0, 1.0), False),   # textured with tex.bmp
]


def build_mesh():
    m = Mesh()
    body = m.add(0, (0, 4, 0), (3, 4, 2), 1)          # 体
    m.add(1, (0, 8, 0), (1, 4, 1), 2)                 # 腕
    m.add(1, (0, 12, 0), (1, 4, 1), 3)                # ひじ
    m.add(0, (4, 2, 0), (1.5, 1.5, 1.5), 11)          # 半付与
    m.add(2, (8, 7, 0), (1, 6, 1), 8)                 # 補助根 (follows 補助IK)
    m.add(2, (9.5, 10, 0), (2, 1, 1), 10)             # 足D
    m.add(2, (-6, 9, 0), (1, 6, 1), 12)               # 揺れ
    m.add(1, (-3, 2, 1), (1, 1, 1), 14)               # 移動付与
    m.add(3, (0, -0.25, 0), (20, 0.5, 6), 0)          # textured base plate on センター
    return m, body


def write_pmx(path):
    mesh, body = build_mesh()
    out = bytearray()
    out += b'PMX ' + f32(2.0)
    # 8 globals: UTF-16, 1 extra UV, index sizes: vertex 2, texture 1,
    # material 1, bone 1, morph 1, rigid body 1
    out += u8(8) + bytes([0, 1, 2, 1, 1, 1, 1, 1])
    out += pmx_text('MMPテスト') + pmx_text('MMP test')
    out += pmx_text('テスト用のモデル') + pmx_text('model for the tests')

    # vertices (BDEF1; the first one is BDEF2 and the second SDEF so that
    # the other weight formats are read as well)
    out += i32(len(mesh.verts))
    for i, (pos, normal, uv, bone) in enumerate(mesh.verts):
        out += f32(*pos) + f32(*normal) + f32(*uv)
        out += f32(uv[0], uv[1], 0.5, 1.0)  # extra UV1
        if i == 0:
            out += u8(1) + i8(bone) + i8(0) + f32(1.0)            # BDEF2
        elif i == 1:
            out += u8(3) + i8(bone) + i8(0) + f32(1.0)            # SDEF
            out += f32(0, 2, 0) + f32(0, 2, 0) + f32(0, 6, 0)
        else:
            out += u8(0) + i8(bone)                               # BDEF1
        out += f32(1.0)  # edge scale

    tris = mesh.ordered_tris()
    out += i32(len(tris) * 3)
    for t in tris:
        out += u16(t[0]) + u16(t[1]) + u16(t[2])

    out += i32(1) + pmx_text('tex.bmp')  # textures

    out += i32(len(PMX_MATERIALS))
    for k, (name, diffuse, edge) in enumerate(PMX_MATERIALS):
        out += pmx_text(name) + pmx_text('mat%d' % k)
        out += f32(*diffuse) + f32(0.3, 0.3, 0.3) + f32(10.0)
        out += f32(diffuse[0] * 0.5, diffuse[1] * 0.5, diffuse[2] * 0.5)
        flags = 0x01 | 0x02 | 0x04 | 0x08 | (0x10 if edge else 0)  # both sides, shadows, edge
        out += u8(flags)
        out += f32(0.0, 0.0, 0.0, 1.0) + f32(1.0)  # edge colour, size
        out += i8(0 if name == '床' else -1) + i8(-1)  # texture, sphere
        out += u8(0)  # sphere mode
        out += u8(1) + u8(k)  # shared toon toon01..toon03
        out += pmx_text('')
        out += i32(len(mesh.material_tris[k]) * 3)

    out += i32(len(PMX_BONES))
    for name, pos, parent, level, flags, extra in PMX_BONES:
        out += pmx_text(name) + pmx_text('')
        out += f32(*pos) + i8(parent) + i32(level) + u16(flags)
        out += f32(0, 1, 0)  # display: coordinate offset
        if 'give' in extra:
            src, rate = extra['give']
            out += i8(src) + f32(rate)
        if 'ik' in extra:
            target, loops, limit, links = extra['ik']
            out += i8(target) + i32(loops) + f32(limit) + i32(len(links))
            for link, lim in links:
                out += i8(link)
                if lim is None:
                    out += u8(0)
                else:
                    out += u8(1) + f32(*lim[0]) + f32(*lim[1])

    # morphs: 0 上 (vertex), 1 色 (material, add), 2 暗く (material, multiply,
    # all materials), 3 骨 (bone), 4 UV (uv), 5 グループ (group)
    # every vertex at the top of the 体 box (top face and the upper corners of
    # the sides), so that the box stretches
    top = [body + k for k in range(24) if mesh.verts[body + k][0][1] == 6]
    morphs = []  # name, panel, type, offset count, data
    morphs.append(('上', 2, 1, len(top), b''.join(u16(v) + f32(0, 2, 0) for v in top)))
    mat_add = (i8(0) + u8(1) + f32(0, 0, 1, 0) + f32(0, 0, 0) + f32(0) + f32(0, 0, 0)
               + f32(0, 0, 0, 0) + f32(0) + f32(0, 0, 0, 0) * 3)
    morphs.append(('色', 3, 8, 1, mat_add))
    mat_mul = (i8(-1) + u8(0) + f32(0.5, 0.5, 0.5, 1) + f32(1, 1, 1) + f32(1) + f32(1, 1, 1)
               + f32(1, 1, 1, 1) + f32(1) + f32(1, 1, 1, 1) * 3)
    morphs.append(('暗く', 3, 8, 1, mat_mul))
    morphs.append(('骨', 4, 2, 1, i8(1) + f32(1, 0, 0) + f32(0, 0, 0, 1)))
    morphs.append(('UV', 3, 3, 1, u16(body + 16) + f32(0.25, 0.5, 0, 0)))
    morphs.append(('グループ', 4, 0, 2, i8(0) + f32(0.5) + i8(2) + f32(1.0)))
    out += i32(len(morphs))
    for name, panel, mtype, count, data in morphs:
        out += pmx_text(name) + pmx_text('') + u8(panel) + u8(mtype) + i32(count) + data

    # display frames
    frames = [
        ('Root', 1, [(0, 0)]),
        ('表情', 1, [(1, 0), (1, 1), (1, 5)]),
        ('体', 0, [(0, 1), (0, 2), (0, 3), (0, 5), (0, 6)]),
    ]
    out += i32(len(frames))
    for name, special, elems in frames:
        out += pmx_text(name) + pmx_text('') + u8(special) + i32(len(elems))
        for target, index in elems:
            out += u8(target) + i8(index)

    # rigid bodies: name, bone, group, mask, shape, size, pos, mode
    bodies = [
        ('体', 1, 0, 0xFFFF, 1, (1.5, 2, 1), (0, 4, 0), 0),
        ('揺れ', 12, 1, 0xFFFF & ~(1 << 2), 2, (0.5, 5, 0), (-6, 9, 0), 1),
        ('揺れ根', 0, 2, 0xFFFF & ~(1 << 1), 0, (0.5, 0, 0), (-6, 12, 0), 0),
    ]
    out += i32(len(bodies))
    for name, bone, group, mask, shape, size, pos, mode in bodies:
        out += pmx_text(name) + pmx_text('') + i8(bone) + u8(group) + u16(mask) + u8(shape)
        out += f32(*size) + f32(*pos) + f32(0, 0, 0)
        out += f32(1.0, 0.5, 0.5, 0.0, 0.5)  # mass, move/rotation decay, elasticity, friction
        out += u8(mode)

    out += i32(1)  # joints
    out += pmx_text('揺れJ') + pmx_text('') + u8(0) + i8(2) + i8(1)
    out += f32(-6, 12, 0) + f32(0, 0, 0)
    out += f32(0, 0, 0) + f32(0, 0, 0)            # movement limits
    out += f32(-1, -1, -1) + f32(1, 1, 1)         # rotation limits
    out += f32(0, 0, 0) + f32(0, 0, 0)            # springs

    with open(path, 'wb') as fp:
        fp.write(out)


# --------------------------------------------------------------------------
# PMD model

def write_pmd(path):
    m = Mesh()
    m.add(0, (0, 4, 0), (3, 4, 2), 1)       # 上半身
    m.add(1, (0, 8, 0), (1, 4, 1), 2)       # 首 (回転影響下 of 上半身)
    m.add(1, (3, 4, 0), (1, 4, 1), 3)       # ひざ
    out = bytearray()
    out += b'Pmd' + f32(1.0) + sjis('PMDテスト', 20) + sjis('テスト用', 256)
    out += u32(len(m.verts))
    for pos, normal, uv, bone in m.verts:
        out += f32(*pos) + f32(*normal) + f32(*uv) + u16(bone) + u16(0) + u8(100) + u8(0)
    tris = m.ordered_tris()
    out += u32(len(tris) * 3)
    for t in tris:
        out += u16(t[0]) + u16(t[1]) + u16(t[2])
    mats = [((0.9, 0.2, 0.2), 1.0), ((0.2, 0.8, 0.3), 0.98)]
    out += u32(len(mats))
    for k, (rgb, alpha) in enumerate(mats):
        out += f32(*rgb) + f32(alpha) + f32(5.0) + f32(0.3, 0.3, 0.3) + f32(0.4, 0.4, 0.4)
        out += u8(k) + u8(1) + u32(len(m.material_tris[k]) * 3) + sjis('', 20)
    # bones: name, parent, tail, type, ik parent, position
    bones = [
        ('センター', 0xFFFF, 1, 1, 0, (0, 0, 0)),
        ('上半身', 0, 2, 0, 0, (0, 2, 0)),
        ('首', 0, 0, 5, 1, (0, 6, 0)),      # 回転影響下: follows 上半身
        ('右ひざ', 0, 4, 0, 0, (3, 6, 0)),
        ('右足首', 3, 0, 6, 0, (3, 2, 0)),
        ('右足ＩＫ', 0, 4, 2, 0, (3, 2, 0)),
    ]
    out += u16(len(bones))
    for name, parent, tail, btype, ikp, pos in bones:
        out += sjis(name, 20) + u16(parent) + u16(tail) + u8(btype) + u16(ikp) + f32(*pos)
    out += u16(1)  # IK: bone, target, chain length, iterations, weight, chain
    out += u16(5) + u16(4) + u8(1) + u16(15) + f32(0.5) + u16(3)
    # morphs: base + 1
    top = list(range(16, 20))
    out += u16(2)
    out += sjis('base', 20) + u32(len(top)) + u8(0)
    for v in top:
        out += u32(v) + f32(*m.verts[v][0])
    out += sjis('上', 20) + u32(len(top)) + u8(3)
    for k in range(len(top)):
        out += u32(k) + f32(0, 1, 0)
    out += u8(1) + u16(1)                     # morph display list
    out += u8(1) + sjis('体\n', 50)           # bone display frame names
    out += u32(2) + u16(1) + u8(1) + u16(2) + u8(1)
    out += u8(0)                              # no English names
    for k in range(10):
        out += sjis('toon%02d.bmp' % (k + 1), 100)
    # rigid bodies (positions relative to the bone)
    out += u32(2)
    out += sjis('上半身', 20) + u16(1) + u8(0) + u16(0xFFFF) + u8(1) + f32(1.5, 2, 1)
    out += f32(0, 2, 0) + f32(0, 0, 0) + f32(1, 0.5, 0.5, 0, 0.5) + u8(0)
    out += sjis('首', 20) + u16(2) + u8(1) + u16(0xFFFE) + u8(0) + f32(0.5, 0, 0)
    out += f32(0, 2, 0) + f32(0, 0, 0) + f32(1, 0.5, 0.5, 0, 0.5) + u8(1)
    out += u32(1)
    out += sjis('首J', 20) + u32(0) + u32(1) + f32(0, 6, 0) + f32(0, 0, 0)
    out += f32(0, 0, 0) * 2 + f32(-0.5, -0.5, -0.5) + f32(0.5, 0.5, 0.5) + f32(0, 0, 0) * 2
    with open(path, 'wb') as fp:
        fp.write(out)


# --------------------------------------------------------------------------
# VMD motion

LINEAR = (20, 20, 107, 107)


def bone_bezier(x=LINEAR, y=LINEAR, z=LINEAR, r=LINEAR):
    """64-byte VMD interpolation block; each curve is (x1, y1, x2, y2)."""
    b = [0] * 64
    curves = [x, y, z, r]
    for c, (x1, y1, x2, y2) in enumerate(curves):
        b[0 + c], b[4 + c], b[8 + c], b[12 + c] = x1, y1, x2, y2
    # rows 2-4 repeat the previous row shifted by one byte, as MMD writes
    # them (only the first row is used when reading)
    for row in range(1, 4):
        for k in range(15):
            b[row * 16 + k] = b[(row - 1) * 16 + k + 1]
    return bytes(b)


EASE_IN_OUT = (64, 0, 64, 127)
EASE_OUT = (0, 64, 64, 127)


def bone_key(name, frame, pos=(0, 0, 0), rot=(0, 0, 0, 1), bezier=None):
    return (sjis(name, 15) + u32(frame) + f32(*pos) + f32(*rot)
            + (bezier if bezier is not None else bone_bezier()))


def write_motion(path):
    keys = [
        bone_key('センター', 0),
        bone_key('センター', 30, (4, 0, 0), bezier=bone_bezier(x=EASE_IN_OUT)),
        bone_key('センター', 60, (0, 0, 0), bezier=bone_bezier(x=EASE_OUT)),
        bone_key('体', 0),
        bone_key('体', 40, rot=quat_axis_angle((0, 1, 0), 60), bezier=bone_bezier(r=EASE_IN_OUT)),
        bone_key('腕IK', 0),
        bone_key('腕IK', 30, (3, -2, 0)),
        bone_key('腕IK', 60, (-2, -3, 0), bezier=bone_bezier(x=EASE_OUT, y=EASE_IN_OUT)),
        bone_key('足', 0),
        bone_key('足', 30, rot=quat_axis_angle((0, 0, 1), 50)),
        bone_key('足', 60, rot=quat_axis_angle((0, 0, 1), -30)),
        # a key the test model does not have (must be ignored)
        bone_key('存在しない', 10, (1, 1, 1)),
    ]
    morphs = [
        ('上', 0, 0.0), ('上', 30, 1.0), ('上', 60, 0.0),
        ('色', 0, 0.0), ('色', 20, 1.0),
        ('骨', 0, 0.0), ('骨', 60, 1.0),
        ('グループ', 40, 0.0), ('グループ', 50, 1.0),
    ]
    out = bytearray()
    out += sjis('Vocaloid Motion Data 0002', 30) + sjis('MMPテスト', 20)
    out += u32(len(keys)) + b''.join(keys)
    out += u32(len(morphs))
    for name, frame, value in morphs:
        out += sjis(name, 15) + u32(frame) + f32(value)
    out += u32(0) + u32(0) + u32(0)  # camera, light, self shadow
    # show/IK: 腕IK is switched off from frame 50
    out += u32(2)
    out += u32(0) + u8(1) + u32(1) + sjis('腕IK', 20) + u8(1)
    out += u32(50) + u8(1) + u32(1) + sjis('腕IK', 20) + u8(0)
    with open(path, 'wb') as fp:
        fp.write(out)


def camera_key(frame, distance, pos, rot, fov, curves=None, ortho=False):
    interp = bytearray()
    for c in (curves or [LINEAR] * 6):
        x1, y1, x2, y2 = c
        interp += bytes([x1, x2, y1, y2])  # stored as x1, x2, y1, y2
    return (u32(frame) + f32(distance) + f32(*pos) + f32(*rot) + bytes(interp)
            + u32(fov) + u8(1 if ortho else 0))


def write_camera(path):
    cams = [
        camera_key(0, -40, (0, 8, 0), (0.1, 0, 0), 30),
        camera_key(30, -30, (2, 8, 0), (0.2, 0.5, 0), 40,
                   curves=[EASE_IN_OUT, LINEAR, LINEAR, EASE_OUT, LINEAR, LINEAR]),
        camera_key(31, -50, (0, 6, 0), (0.0, -0.5, 0), 30),   # cut (one frame apart)
        camera_key(60, -45, (0, 7, 0), (0.1, 0, 0), 30),
    ]
    lights = [
        (0, (0.6, 0.6, 0.6), (-0.5, -1.0, 0.5)),
        (60, (1.0, 0.8, 0.6), (0.5, -1.0, 0.5)),
    ]
    out = bytearray()
    out += sjis('Vocaloid Motion Data 0002', 30) + sjis('カメラ・照明', 20)
    out += u32(0) + u32(0)
    out += u32(len(cams)) + b''.join(cams)
    out += u32(len(lights))
    for frame, color, pos in lights:
        out += u32(frame) + f32(*color) + f32(*pos)
    out += u32(1) + u32(0) + u8(1) + f32(0.01)  # self shadow mode 1
    out += u32(0)
    with open(path, 'wb') as fp:
        fp.write(out)


def write_texture(path):
    """16x16 24-bit BMP checkerboard (4x4 cells)."""
    w = h = 16
    rows = bytearray()
    for y in range(h):  # bottom-up
        for x in range(w):
            if ((x // 4) + (y // 4)) % 2:
                rows += bytes((200, 200, 200))   # BGR
            else:
                rows += bytes((90, 140, 230))
    header = b'BM' + u32(54 + len(rows)) + u32(0) + u32(54)
    info = u32(40) + i32(w) + i32(h) + u16(1) + u16(24) + u32(0) + u32(len(rows))
    info += i32(2835) + i32(2835) + u32(0) + u32(0)
    with open(path, 'wb') as fp:
        fp.write(header + info + rows)


def main():
    if len(sys.argv) != 2:
        sys.stderr.write('usage: %s OUTPUT_DIR\n' % sys.argv[0])
        return 2
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    write_texture(os.path.join(out, 'tex.bmp'))
    write_pmx(os.path.join(out, 'test_model.pmx'))
    write_pmd(os.path.join(out, 'test_model.pmd'))
    write_motion(os.path.join(out, 'test_motion.vmd'))
    write_camera(os.path.join(out, 'test_camera.vmd'))
    return 0


if __name__ == '__main__':
    sys.exit(main())

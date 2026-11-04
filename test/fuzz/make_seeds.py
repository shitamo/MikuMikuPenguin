#!/usr/bin/env python3
"""Generate small PMX / PMD / VMD files used as fuzzing seeds.

    python3 test/fuzz/make_seeds.py [output-dir]   (default: test/fuzz/seeds)

The models are synthetic (a few triangles), but use every feature the
loaders know: all PMX weight types and morph types, index sizes 1/2/4,
UTF-16 and UTF-8 text, extra UVs, IK with angle limits, rotation/translation
inheritance, fixed and local axes, display frames, all rigid body shapes and
modes, joints; PMD IK, skins, display lists, English names, toon names and
physics; VMD bone/morph/camera/light/self shadow/show-IK keys.  Bone and
morph names match the built-in model of the VMD fuzz target.
"""

import os
import struct
import sys

# ------------------------------------------------------------------ helpers


def f32(*v):
    return struct.pack('<%df' % len(v), *v)


def i32(v):
    return struct.pack('<i', v)


def u32(v):
    return struct.pack('<I', v)


def u16(v):
    return struct.pack('<H', v)


def u8(v):
    return struct.pack('<B', v)


def sjis(text, size):
    b = text.encode('cp932')[:size]
    return b + b'\0' * (size - len(b))


# ------------------------------------------------------------------ PMX

BONES = [  # name, position, parent
    ('センター', (0, 8, 0), -1),
    ('上半身', (0, 12, 0), 0),
    ('左足', (1, 10, 0), 0),
    ('左ひざ', (1, 5, 0), 2),
    ('左足首', (1, 1, 0), 3),
    ('左足ＩＫ', (1, 1, 0), -1),
    ('付与', (0, 13, 0), 1),
    ('髪', (0, 14, -1), 1),
    ('後', (0, 15, 0), 1),
]


class PMXWriter:
    def __init__(self, utf8, sizes, extra_uv):
        self.utf8 = utf8
        # vertex, texture, material, bone, morph, rigid body index sizes
        self.vs, self.ts, self.ms, self.bs, self.mos, self.rs = sizes
        self.extra_uv = extra_uv

    def text(self, s):
        b = s.encode('utf-8' if self.utf8 else 'utf-16-le')
        return i32(len(b)) + b

    @staticmethod
    def index(v, size, unsigned=False):
        fmt = {1: '<B', 2: '<H', 4: '<i'} if unsigned else {1: '<b', 2: '<h', 4: '<i'}
        return struct.pack(fmt[size], v)

    def vidx(self, v):
        return self.index(v, self.vs, unsigned=True)

    def bidx(self, v):
        return self.index(v, self.bs)

    def build(self):
        out = bytearray(b'PMX ' + f32(2.0) + u8(8))
        out += bytes([1 if self.utf8 else 0, self.extra_uv,
                      self.vs, self.ts, self.ms, self.bs, self.mos, self.rs])
        out += self.text('テスト') + self.text('test')
        out += self.text('コメント\r\n') + self.text('comment')

        # vertices: two of each weight type
        nb = len(BONES)
        verts = bytearray()
        nv = 8
        for i in range(nv):
            verts += f32(i % 2, i, 0.5) + f32(0, 0, -1) + f32(i % 2, i / 8.0)
            for k in range(self.extra_uv):
                verts += f32(0.1 * k, 0.2, 0.3, 0.4)
            w = i % 4
            verts += u8(w)
            if w == 0:
                verts += self.bidx(i % nb)
            elif w == 1:
                verts += self.bidx(i % nb) + self.bidx((i + 1) % nb) + f32(0.7)
            elif w == 2:
                verts += b''.join(self.bidx((i + k) % nb) for k in range(4))
                verts += f32(0.4, 0.3, 0.2, 0.1)
            else:
                verts += self.bidx(1) + self.bidx(2) + f32(0.6)
                verts += f32(0, 11, 0) + f32(0, 12, 0) + f32(0, 10, 0)
            verts += f32(1.0)
        out += i32(nv) + verts

        faces = [0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4, 4, 5, 6, 5, 7, 6]
        out += i32(len(faces)) + b''.join(self.vidx(v) for v in faces)

        textures = ['tex\\body.png', 'sph.sph', 'toon.bmp']
        out += i32(len(textures)) + b''.join(self.text(t) for t in textures)

        # materials: 9 + 9 indices
        mats = bytearray()
        for i, (tex, sph, mode, shared) in enumerate([(0, 1, 1, 1), (-1, -1, 0, 0)]):
            mats += self.text('材質%d' % i) + self.text('mat%d' % i)
            mats += f32(1, 1, 1, 0.9) + f32(0.5, 0.5, 0.5) + f32(5) + f32(0.6, 0.6, 0.6)
            mats += u8(0x1f) + f32(0, 0, 0, 1) + f32(1.0)
            mats += self.index(tex, self.ts) + self.index(sph, self.ts) + u8(mode) + u8(shared)
            mats += u8(3) if shared else self.index(2, self.ts)
            mats += self.text('memo') + i32(9)
        out += i32(2) + mats

        # bones
        bones = bytearray()
        for i, (name, pos, parent) in enumerate(BONES):
            bones += self.text(name) + self.text('bone%d' % i)
            bones += f32(*pos) + self.bidx(parent) + i32(1 if name == '後' else 0)
            flags = 0x02 | 0x08 | 0x10
            if i in (0, 5):
                flags |= 0x04  # movable
            if i == 2:
                flags |= 0x01  # tail is a bone
            if name == '左足ＩＫ':
                flags |= 0x20
            if name == '付与':
                flags |= 0x100 | 0x200 | 0x400 | 0x800 | 0x2000
            if name == '後':
                flags |= 0x1000
            bones += u16(flags)
            bones += self.bidx(3) if flags & 0x01 else f32(0, -1, 0)
            if flags & 0x300:
                bones += self.bidx(1) + f32(0.5)
            if flags & 0x400:
                bones += f32(0, 1, 0)
            if flags & 0x800:
                bones += f32(1, 0, 0) + f32(0, 0, 1)
            if flags & 0x2000:
                bones += i32(0)
            if flags & 0x20:
                bones += self.bidx(4) + i32(40) + f32(2.0) + i32(2)
                bones += self.bidx(3) + u8(1) + f32(-3.14, 0, 0) + f32(-0.01, 0, 0)
                bones += self.bidx(2) + u8(0)
        out += i32(len(BONES)) + bones

        # morphs: vertex, UV, extra UV1, bone, material (one / all), group
        morphs = bytearray()
        midx = lambda v: self.index(v, self.mos)
        morphs += self.text('あ') + self.text('a') + u8(3) + u8(1) + i32(2)
        morphs += self.vidx(0) + f32(0.1, 0, 0) + self.vidx(1) + f32(0, 0.1, 0)
        morphs += self.text('uv') + self.text('uv') + u8(4) + u8(3) + i32(1)
        morphs += self.vidx(2) + f32(0.1, 0.1, 0, 0)
        morphs += self.text('uv1') + self.text('uv1') + u8(4) + u8(4) + i32(1)
        morphs += self.vidx(3) + f32(0.1, 0.2, 0.3, 0.4)
        morphs += self.text('bone') + self.text('bone') + u8(4) + u8(2) + i32(1)
        morphs += self.bidx(1) + f32(0, 1, 0) + f32(0, 0.38, 0, 0.92)
        for name, idx, op in (('mat', 0, 0), ('matall', -1, 1)):
            morphs += self.text(name) + self.text(name) + u8(4) + u8(8) + i32(1)
            morphs += self.index(idx, self.ms) + u8(op)
            morphs += f32(0.5, 0.5, 0.5, 1) + f32(1, 1, 1) + f32(2) + f32(1, 1, 1)
            morphs += f32(0, 0, 0, 1) + f32(1) + f32(1, 1, 1, 1) * 3
        morphs += self.text('group') + self.text('group') + u8(4) + u8(0) + i32(3)
        morphs += midx(0) + f32(0.5) + midx(3) + f32(1.0) + midx(4) + f32(0.3)
        out += i32(7) + morphs

        # display frames
        frames = bytearray()
        frames += self.text('Root') + self.text('Root') + u8(1) + i32(1) + u8(0) + self.bidx(0)
        frames += self.text('表情') + self.text('Exp') + u8(1) + i32(2) + u8(1) + midx(0) + u8(1) + midx(6)
        frames += self.text('足') + self.text('Leg') + u8(0) + i32(2) + u8(0) + self.bidx(2) + u8(0) + self.bidx(5)
        out += i32(3) + frames

        # rigid bodies: capsule follower, sphere physics, box physics+bone, free sphere
        rbs = bytearray()
        for i, (bone, shape, mode, pos) in enumerate([(1, 2, 0, (0, 12, 0)), (7, 0, 1, (0, 14, -1)),
                                                      (8, 1, 2, (0, 15, 0)), (-1, 0, 1, (0, 20, 0))]):
            rbs += self.text('剛体%d' % i) + self.text('rb%d' % i) + self.bidx(bone)
            rbs += u8(i) + u16(0xffff & ~(1 << i)) + u8(shape)
            rbs += f32(1, 2, 1) + f32(*pos) + f32(0.1, 0.2, 0.3)
            rbs += f32(1, 0.5, 0.5, 0, 0.5) + u8(mode)
        out += i32(4) + rbs

        joints = bytearray()
        for i, (a, b) in enumerate([(0, 1), (2, 1), (3, -1)]):
            joints += self.text('J%d' % i) + self.text('j%d' % i) + u8(0)
            joints += self.index(a, self.rs) + self.index(b, self.rs)
            joints += f32(0, 13, -1) + f32(0, 0, 0)
            joints += f32(0, 0, 0) * 2 + f32(-0.5, -0.5, -0.5) + f32(0.5, 0.5, 0.5)
            joints += f32(0, 0, 0) + f32(10, 10, 10)
        out += i32(3) + joints
        return bytes(out)


# ------------------------------------------------------------------ PMD

def make_pmd():
    out = bytearray(b'Pmd' + f32(1.0) + sjis('テスト', 20) + sjis('PMD seed', 256))
    names = ['センター', '上半身', '左足', '左ひざ', '左足首', '左足ＩＫ', '髪', '目', '捩', '連動']
    # bone types: 1 rot+move, 0, 0, 0, 0, 2 IK, 0, 5 under rotation, 8 twist, 9 rotation link
    bones = [(0xffff, 1, 1, 0), (0, 6, 0, 0), (0, 3, 0, 0), (2, 4, 0, 0), (3, 0, 6, 0),
             (0xffff, 4, 2, 0), (1, 0, 0, 0), (1, 0, 5, 1), (1, 6, 8, 0), (1, 1, 9, 50)]
    pos = [(0, 8, 0), (0, 12, 0), (1, 10, 0), (1, 5, 0), (1, 1, 0),
           (1, 1, 0), (0, 14, -1), (0, 16, 0), (0, 13, 0), (0, 13, 1)]

    nv = 8
    out += u32(nv)
    for i in range(nv):
        out += f32(i % 2, i, 0.5) + f32(0, 0, -1) + f32(i % 2, i / 8.0)
        out += u16(i % len(names)) + u16((i + 1) % len(names)) + u8(70) + u8(i % 2)
    faces = [0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4, 4, 5, 6, 5, 7, 6]
    out += u32(len(faces)) + b''.join(u16(v) for v in faces)

    out += u32(3)
    for i, (toon, tex) in enumerate([(0, b'body.bmp*sph.sph'), (0xff, b'spa.spa'), (3, b'')]):
        out += f32(1, 1, 1, 0.98 if i == 1 else 1.0) + f32(5) + f32(0.5, 0.5, 0.5) + f32(0.6, 0.6, 0.6)
        out += u8(toon) + u8(1) + u32(6) + tex + b'\0' * (20 - len(tex))

    out += u16(len(names))
    for n, (parent, tail, typ, ikp), p in zip(names, bones, pos):
        out += sjis(n, 20) + u16(parent) + u16(tail) + u8(typ) + u16(ikp) + f32(*p)

    out += u16(1)  # IK: 左足ＩＫ -> 左足首 via 左ひざ, 左足
    out += u16(5) + u16(4) + u8(2) + u16(40) + f32(0.5) + u16(3) + u16(2)

    out += u16(3)  # skins: base, あ, い
    out += sjis('base', 20) + u32(3) + u8(0)
    for v in range(3):
        out += u32(v) + f32(v % 2, v, 0.5)
    out += sjis('あ', 20) + u32(2) + u8(3) + u32(0) + f32(0.1, 0, 0) + u32(2) + f32(0, 0.1, 0)
    out += sjis('い', 20) + u32(1) + u8(1) + u32(1) + f32(0, 0, 0.1)

    out += u8(2) + u16(1) + u16(2)  # skin display list
    out += u8(2) + sjis('体\n', 50) + sjis('足\n', 50)  # bone display names
    out += u32(4) + u16(1) + u8(1) + u16(2) + u8(2) + u16(3) + u8(2) + u16(5) + u8(2)

    out += u8(1) + sjis('test', 20) + sjis('english comment', 256)  # English names
    out += b''.join(sjis('bone%d' % i, 20) for i in range(len(names)))
    out += sjis('a', 20) + sjis('i', 20)
    out += sjis('Body', 50) + sjis('Leg', 50)

    out += b''.join(sjis('toon%02d.bmp' % (i + 1), 100) for i in range(10))

    out += u32(3)  # rigid bodies: capsule follower, sphere physics, box physics+bone
    for i, (bone, shape, mode) in enumerate([(1, 2, 0), (6, 0, 1), (0xffff, 1, 2)]):
        out += sjis('剛体%d' % i, 20) + u16(bone) + u8(i) + u16(0xffff & ~(1 << i)) + u8(shape)
        out += f32(1, 2, 1) + f32(0, 0.5, 0) + f32(0.1, 0.2, 0.3) + f32(1, 0.5, 0.5, 0, 0.5) + u8(mode)
    out += u32(2)
    for i, (a, b) in enumerate([(0, 1), (1, 2)]):
        out += sjis('J%d' % i, 20) + u32(a) + u32(b) + f32(0, 13, -1) + f32(0, 0, 0)
        out += f32(0, 0, 0) * 2 + f32(-0.5, -0.5, -0.5) + f32(0.5, 0.5, 0.5) + f32(0, 0, 0) + f32(10, 10, 10)
    return bytes(out)


# ------------------------------------------------------------------ VMD

def bezier_block():
    # 64 bytes: X/Y/Z/R x1 y1 x2 y2 in MMD's layout (repeated rows)
    row = bytes([20, 20, 20, 20, 20, 20, 20, 20, 107, 107, 107, 107, 107, 107, 107, 107])
    return (row * 4)[:64]


def make_vmd(camera_only=False):
    out = bytearray(sjis('Vocaloid Motion Data 0002', 30))
    out += sjis('' if camera_only else 'テスト', 20)
    if camera_only:
        out += u32(0) + u32(0)
    else:
        keys = []
        for name in ['センター', '上半身', '左足ＩＫ', '付与', '髪', '後', '左足']:
            for frame in (0, 10, 30):
                keys.append((name, frame))
        out += u32(len(keys))
        for name, frame in keys:
            out += sjis(name, 15) + u32(frame) + f32(0, frame * 0.1, 0) + f32(0, 0.1 * (frame % 7), 0, 1)
            out += bezier_block()
        morphs = [('あ', 0, 0.0), ('あ', 5, 1.0), ('group', 8, 1.0), ('mat', 3, 0.5), ('matall', 6, 1.0),
                  ('bone', 2, 1.0), ('uv', 1, 1.0), ('uv1', 4, 1.0), ('い', 5, 1.0)]
        out += u32(len(morphs))
        for name, frame, value in morphs:
            out += sjis(name, 15) + u32(frame) + f32(value)
    cams = [(0, 45.0), (1, 40.0), (30, 20.0)]
    out += u32(len(cams))
    for frame, dist in cams:
        out += u32(frame) + f32(-dist) + f32(0, 10, 0) + f32(0.1, 0.2, 0.0)
        out += bytes([20, 107, 20, 107] * 6) + u32(30) + u8(0)
    out += u32(2) + u32(0) + f32(0.6, 0.6, 0.6) + f32(-0.5, -1, 0.5)
    out += u32(20) + f32(1, 0.5, 0.5) + f32(0.5, -1, -0.5)
    out += u32(2) + u32(0) + u8(1) + f32(0.01) + u32(15) + u8(2) + f32(0.02)
    if camera_only:
        out += u32(0)
    else:
        out += u32(2)
        out += u32(0) + u8(1) + u32(1) + sjis('左足ＩＫ', 20) + u8(1)
        out += u32(12) + u8(0) + u32(1) + sjis('左足ＩＫ', 20) + u8(0)
    return bytes(out)


def make_old_vmd():
    # version 1 header, bone keys only, file ends after the morph count
    out = bytearray(sjis('Vocaloid Motion Data file', 30) + sjis('old', 10))
    out += u32(1) + sjis('センター', 15) + u32(0) + f32(0, 1, 0) + f32(0, 0, 0, 1) + bezier_block()
    return bytes(out)


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'seeds')
    files = {
        'pmx/utf16_index1.pmx': PMXWriter(False, (1, 1, 1, 1, 1, 1), 0).build(),
        'pmx/utf8_index2_uv1.pmx': PMXWriter(True, (2, 2, 2, 2, 2, 2), 1).build(),
        'pmx/utf16_index4_uv4.pmx': PMXWriter(False, (4, 4, 4, 4, 4, 4), 4).build(),
        'pmd/model.pmd': make_pmd(),
        'vmd/motion.vmd': make_vmd(),
        'vmd/camera.vmd': make_vmd(camera_only=True),
        'vmd/old_v1.vmd': make_old_vmd(),
    }
    for name, data in files.items():
        path = os.path.join(root, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as f:
            f.write(data)
        print('%s (%d bytes)' % (path, len(data)))
    # readPMX also accepts PMD data
    with open(os.path.join(root, 'pmx/model.pmd'), 'wb') as f:
        f.write(files['pmd/model.pmd'])


if __name__ == '__main__':
    main()

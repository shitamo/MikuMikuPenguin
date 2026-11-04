#!/usr/bin/env python3
"""Generate the loader regression files in this directory.

    python3 test/regress/make_regress.py

Each file is a minimal PMX / PMD / VMD that reproduces one problem found by
fuzzing (test/fuzz) or by reviewing the loaders; the comment of each case
says what used to happen.  Files named *.reject.* must be rejected with
std::runtime_error, the others must load (with the invalid parts repaired).
test/regress_test.cpp runs them (meson test).
"""

import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))


def f32(*v):
    return struct.pack('<%df' % len(v), *v)


def i32(*v):
    return struct.pack('<%di' % len(v), *v)


def u32(v):
    return struct.pack('<I', v)


def u16(v):
    return struct.pack('<H', v)


def u8(*v):
    return bytes(v)


def i8(v):
    return struct.pack('<b', v)


def sjis(text, size):
    b = text.encode('cp932')[:size]
    return b + b'\0' * (size - len(b))


# ---------------------------------------------------------------- PMX
# index sizes are all 1 byte; text is UTF-8

def text(s):
    b = s.encode('utf-8')
    return i32(len(b)) + b


def pmx(vertices=None, faces=None, textures=None, materials=None, bones=None, morphs=None,
        frames=None, bodies=None, joints=None, utf16=False, names=None):
    """A PMX 2.0 file; each section is (count, bytes) or None for an empty one."""
    out = b'PMX ' + f32(2.0) + u8(8, 0 if utf16 else 1, 0, 1, 1, 1, 1, 1, 1)
    out += names if names is not None else text('regress') + text('') + text('') + text('')
    for section in (vertices, faces, textures, materials, bones, morphs, frames, bodies, joints):
        count, data = section if section else (0, b'')
        out += i32(count) + data
    return out


def vertex_bdef1(bone=0, pos=(0, 0, 0)):
    return f32(*pos) + f32(0, 0, -1) + f32(0, 0) + u8(0) + i8(bone) + f32(1)


def bone(name, parent=-1, pos=(0, 0, 0), flags=0x1e, extra=b''):
    """A bone with a coordinate-offset tail; extra = data of the extra flags."""
    return text(name) + text('') + f32(*pos) + i8(parent) + i32(0) + u16(flags) + f32(0, 1, 0) + extra


def ik_bone(name, target, loops, links, pos=(0, 0, 0)):
    """links: list of (bone index, angle limit byte)"""
    data = i8(target) + u32(loops) + f32(1.0) + i32(len(links))
    for index, limit in links:
        data += i8(index) + u8(limit)
        if limit:
            data += f32(-1, 0, 0) + f32(0, 0, 0)
    return bone(name, -1, pos, 0x1e | 0x20, data)


def material(textures=(-1, -1), toon=(1, 0), faces=0):
    shared, toon_value = toon
    out = text('m') + text('') + f32(1, 1, 1, 1) + f32(0, 0, 0) + f32(1) + f32(0, 0, 0) + u8(0)
    out += f32(0, 0, 0, 1) + f32(1) + i8(textures[0]) + i8(textures[1]) + u8(0) + u8(shared)
    out += u8(toon_value) if shared == 1 else i8(toon_value)
    return out + text('') + i32(faces)


def rigid_body(bone=-1, group=0, shape=0, mode=1, size=(1, 1, 1)):
    return (text('rb') + text('') + i8(bone) + u8(group) + u16(0xffff) + u8(shape) + f32(*size)
            + f32(0, 10, 0) + f32(0, 0, 0) + f32(1, 0.5, 0.5, 0, 0.5) + u8(mode))


def joint(a, b):
    return text('j') + text('') + u8(0) + i8(a) + i8(b) + f32(0, 10, 0) + f32(0, 0, 0) + f32(0) * 18


TWO_BONES = (2, bone('root') + bone('child', 0, (0, 1, 0)))
TRIANGLE = (3, b''.join(vertex_bdef1(0, (i, i % 2, 0)) for i in range(3)))


# ---------------------------------------------------------------- PMD

def pmd(vertices=(0, b''), bones=(0, b''), iks=(0, b''), bodies=None, joints=None, truncate=None):
    out = b'Pmd' + f32(1.0) + sjis('regress', 20) + sjis('', 256)
    out += u32(vertices[0]) + vertices[1]
    out += u32(0)  # faces
    out += u32(0)  # materials
    out += u16(bones[0]) + bones[1]
    out += u16(iks[0]) + iks[1]
    out += u16(0)  # skins
    out += u8(0) + u8(0) + u32(0)  # display lists
    out += u8(0)  # no English names
    out += b''.join(sjis('toon%02d.bmp' % (i + 1), 100) for i in range(10))
    if bodies is not None:
        out += u32(bodies[0]) + bodies[1]
        if joints is not None:
            out += u32(joints[0]) + joints[1]
    return out[:truncate] if truncate else out


def pmd_bone(name, parent=0xffff, pos=(0, 0, 0)):
    return sjis(name, 20) + u16(parent) + u16(0) + u8(0) + u16(0) + f32(*pos)


def pmd_rigid_body(bone=0xffff, group=0, shape=0, mode=1):
    return (sjis('rb', 20) + u16(bone) + u8(group) + u16(0xffff) + u8(shape) + f32(1, 1, 1)
            + f32(0, 10, 0) + f32(0, 0, 0) + f32(1, 0.5, 0.5, 0, 0.5) + u8(mode))


def pmd_joint(a, b):
    return sjis('j', 20) + u32(a) + u32(b) + f32(0, 10, 0) + f32(0, 0, 0) + f32(0) * 18


# ---------------------------------------------------------------- VMD

def vmd(bones=(0, b''), morphs=(0, b''), cameras=(0, b''), lights=(0, b''), shadows=(0, b''), iks=(0, b'')):
    out = sjis('Vocaloid Motion Data 0002', 30) + sjis('regress', 20)
    for count, data in (bones, morphs, cameras, lights, shadows, iks):
        out += u32(count) + data
    return out


def vmd_bone(name, frame):
    return sjis(name, 15) + u32(frame) + f32(0, 0, 0) + f32(0, 0, 0, 1) + bytes([20] * 8 + [107] * 8) * 4


CASES = {
    # ---- PMX
    # The text length was trusted: a 2 GB vector was allocated for a 100-byte file.
    'pmx_text_length.reject.pmx': b'PMX ' + f32(2.0) + u8(8, 1, 0, 1, 1, 1, 1, 1, 1) + u32(0x7ffffff0) + b'x' * 16,
    # A UTF-16 text of odd length overwrote the terminating NUL; the UTF-16 ->
    # UTF-8 conversion then read past the end of the buffer (heap overflow).
    'pmx_utf16_odd_text.pmx': pmx(utf16=True, names=i32(3) + b'A\0B' + i32(0) * 3),
    # A huge vertex count with no vertex data: the loader kept allocating
    # vertices from a failed stream (hang, gigabytes of memory).
    'pmx_vertex_count.reject.pmx': pmx(vertices=(0x7fffffff, b'')),
    # A negative face count was kept; the viewer calloc()ed with it.
    'pmx_negative_count.reject.pmx': pmx(faces=(-6, b'')),
    # 1 million textures accepted in a 60-byte file (32 MB of strings).
    'pmx_texture_count.reject.pmx': pmx(textures=(1000000, b'')),
    # An index count that is not a multiple of 3: the last index was not
    # read, so every later section was misread.
    'pmx_face_remainder.pmx': pmx(vertices=TRIANGLE, faces=(4, u8(0, 1, 2, 0)), textures=(1, text('a.png'))),
    # Face indices past the last vertex went to the GPU index buffer.
    'pmx_face_vertex.pmx': pmx(vertices=TRIANGLE, faces=(3, u8(0, 1, 200))),
    # Faces in a model without vertices (found by the fuzzer after the first
    # fixes: out-of-range indices were replaced with 0, still invalid here).
    'pmx_faces_no_vertices.pmx': pmx(faces=(3, u8(0, 1, 2)), materials=(1, material(faces=3))),
    'pmd_faces_no_vertices.pmd': b'',  # filled in below
    # Material face counts larger than the index buffer (glDrawElements
    # outside the buffer), negative or not a multiple of 3; texture and
    # toon indices outside the texture list.
    'pmx_material_ranges.pmx': pmx(vertices=TRIANGLE, faces=(3, u8(0, 1, 2)), textures=(1, text('a.png')),
                                   materials=(3, material((5, -1), (0, 9), 300) + material((-7, 0), (1, 200), -3)
                                              + material(faces=2))),
    # Vertex bone indices outside the bone list (read by the shader).
    'pmx_vertex_bone.pmx': pmx(vertices=(1, vertex_bdef1(bone=100)), bones=TWO_BONES),
    # IK target bone out of range: VMDMotionController read past the bone
    # list (heap-buffer-overflow in solveIKBone).
    'pmx_ik_target.pmx': pmx(bones=(3, bone('root') + bone('child', 0, (0, 1, 0)) + ik_bone('IK', 100, 10, [(1, 0)]))),
    # IK link bone out of range (heap-buffer-overflow in solveIKBone).
    'pmx_ik_link.pmx': pmx(bones=(3, bone('root') + bone('child', 0, (0, 1, 0)) + ik_bone('IK', 1, 10, [(90, 0)]))),
    # IK link angle limit flag 155 loaded into a bool (undefined behaviour).
    'pmx_ik_bool.pmx': pmx(bones=(3, bone('root') + bone('child', 0, (0, 1, 0)) + ik_bone('IK', 1, 10, [(0, 155)]))),
    # IK loop count 0xffffffff with a target that cannot be reached: the
    # IK solver looped for hours on every frame (now capped at 255 like MMD).
    'pmx_ik_loop_count.pmx': pmx(bones=(3, bone('root') + bone('child', 0, (0, 1, 0))
                                        + ik_bone('IK', 1, 0xffffffff, [(0, 0)], pos=(50, 50, 50)))),
    # Morph offsets referring to missing vertices, bones, materials and
    # morphs, and a group morph containing itself.
    'pmx_morph_indices.pmx': pmx(vertices=TRIANGLE, bones=TWO_BONES, materials=(1, material()), morphs=(5,
        text('v') + text('') + u8(4, 1) + i32(2) + u8(200) + f32(1, 0, 0) + u8(255) + f32(1, 0, 0)
        + text('uv') + text('') + u8(4, 3) + i32(1) + u8(9) + f32(1, 0, 0, 0)
        + text('b') + text('') + u8(4, 2) + i32(1) + i8(50) + f32(0, 0, 0) + f32(0, 0, 0, 1)
        + text('m') + text('') + u8(4, 8) + i32(1) + i8(7) + u8(0) + f32(1) * 28
        + text('g') + text('') + u8(4, 0) + i32(3) + i8(4) + f32(1) + i8(99) + f32(1) + i8(-5) + f32(1))),
    # Display frame elements referring to missing bones / morphs, and a
    # target flag of 2 loaded into a bool.
    'pmx_display_frame.pmx': pmx(bones=TWO_BONES, frames=(1,
        text('f') + text('') + u8(7) + i32(3) + u8(0) + i8(40) + u8(1) + i8(3) + u8(2) + i8(0))),
    # Rigid body collision group 200: "1 << group" in MMDPhysics is undefined
    # behaviour (groups are 0-15).
    'pmx_rigid_group.pmx': pmx(bones=TWO_BONES, bodies=(1, rigid_body(0, group=200, mode=0))),
    # Rigid body attached to bone 100 of 2: MMDPhysics::updateBones() read
    # past the bone list.
    'pmx_rigid_bone.pmx': pmx(bones=TWO_BONES, bodies=(1, rigid_body(100, mode=0))),
    # Unknown rigid body shape: MMDPhysics created no Bullet body for it, so
    # the Bullet and PMX body lists no longer matched (heap-buffer-overflow).
    'pmx_rigid_shape.reject.pmx': pmx(bones=TWO_BONES, bodies=(2, rigid_body(0, shape=7) + rigid_body(1, mode=0))),
    # Non-finite rigid body sizes and masses given to Bullet.
    'pmx_rigid_nan.pmx': pmx(bones=TWO_BONES, bodies=(1, text('rb') + text('') + i8(1) + u8(0) + u16(0xffff) + u8(1)
                                                      + f32(float('nan'), float('inf'), -1) + f32(0, 10, 0)
                                                      + f32(0, float('nan'), 0) + f32(float('-inf'), 0.5, 0.5, 0, 0.5) + u8(1))),
    # Joints without rigid body A (or with B out of range): MMDPhysics read
    # the Bullet body list at -1 / past its end.
    'pmx_joint_body.pmx': pmx(bones=TWO_BONES, bodies=(1, rigid_body(0)), joints=(3, joint(-1, 0) + joint(0, 5) + joint(3, -1))),

    # ---- PMD
    # Joints referring to rigid bodies that do not exist (index -1 in MMDPhysics).
    'pmd_joint_body.pmd': pmd(bones=(1, pmd_bone('root')), bodies=(1, pmd_rigid_body(0)),
                              joints=(2, pmd_joint(5, 0) + pmd_joint(0, 7))),
    # Rigid body group 128 ("1 << group", undefined behaviour).
    'pmd_rigid_group.pmd': pmd(bones=(1, pmd_bone('root')), bodies=(1, pmd_rigid_body(0, group=128))),
    # Unknown rigid body shape (Bullet / PMX body lists out of step).
    'pmd_rigid_shape.reject.pmd': pmd(bones=(1, pmd_bone('root')), bodies=(2, pmd_rigid_body(0, shape=9) + pmd_rigid_body(0, mode=0))),
    # A truncated file: the partly loaded model was leaked on the error.
    'pmd_truncated.reject.pmd': pmd(vertices=(2, f32(0) * 8 + u16(0) + u16(0) + u8(100, 0)), truncate=320),
    # IK iterations 65535 with an unreachable target (capped at 255 now).
    'pmd_ik_iterations.pmd': pmd(bones=(3, pmd_bone('root') + pmd_bone('child', 0, (0, 1, 0)) + pmd_bone('ik', 0xffff, (50, 50, 50))),
                                 iks=(1, u16(2) + u16(1) + u8(1) + u16(65535) + f32(1.0) + u16(0))),

    # ---- VMD
    # A bone frame count of 9,999,999 in a 60-byte file: about 1.5 GB was
    # allocated before reading (the limit was 10,000,000 records).
    'vmd_huge_count.reject.vmd': vmd(bones=(9999999, b'')),
    # Show/IK flags 7 and 9 loaded into bools (undefined behaviour).
    'vmd_show_ik_bool.vmd': vmd(iks=(1, u32(0) + u8(7) + u32(1) + sjis('IK', 20) + u8(9))),
    # IK state count 100,000 in a short file (2 MB of empty names).
    'vmd_ik_count.reject.vmd': vmd(iks=(1, u32(0) + u8(1) + u32(100000))),
    # A file that ends in the middle of a key: the missing values used to be
    # left uninitialised and the frame was used anyway.
    'vmd_truncated_key.reject.vmd': vmd(bones=(2, vmd_bone('root', 0)))[:-1],
    # Not a VMD file (no "Vocaloid Motion Data" header).
    'vmd_not_vmd.reject.vmd': b'PMX ' + b'\0' * 60,
}


def pmd_faces_no_vertices():
    data = bytearray(pmd())
    # replace the face count 0 (right after the vertex count) by 3 indices
    at = 3 + 4 + 20 + 256 + 4
    data[at:at + 4] = u32(3) + u16(0) + u16(1) + u16(2)
    return bytes(data)


CASES['pmd_faces_no_vertices.pmd'] = pmd_faces_no_vertices()


def main():
    for name, data in sorted(CASES.items()):
        with open(os.path.join(HERE, name), 'wb') as f:
            f.write(data)
        print('%-32s %6d bytes' % (name, len(data)))


if __name__ == '__main__':
    main()

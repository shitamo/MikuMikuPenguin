#!/usr/bin/env python3
"""Rendering comparison test for the pmx viewer.

Draws fixed frames of the generated test model with `pmx --snapshot` (Mesa
software rendering, LIBGL_ALWAYS_SOFTWARE=1) and compares them with the
reference images in test/reference using imgdiff (small differences are
tolerated).  Without a display an Xvfb server is started; if there is none,
the test is skipped (exit status 77), or fails when the environment variable
MMP_RENDER_TEST_REQUIRED is set.

Normally run by `meson test` (suite "render").  To regenerate the reference
images after an intended change of the rendering:

    meson compile -C build update-reference-images

(or run this script with --update) and check the new images in
test/reference before committing them.
"""

import argparse
import os
import shutil
import subprocess
import sys

SKIP = 77
SIZE = '640x360'

# name: (model, motion, extra options, frame)
CASES = [
    ('model_frame0', 'test_model.pmx', 'test_motion.vmd', [], 0),
    ('model_frame30', 'test_model.pmx', 'test_motion.vmd', [], 30),
    ('model_frame50_camera', 'test_model.pmx', 'test_motion.vmd', ['-c', '{data}/test_camera.vmd'], 50),
    ('pmd_frame0', 'test_model.pmd', None, [], 0),
]


def start_xvfb():
    """Start Xvfb on a free display; returns (process, display) or None."""
    xvfb = shutil.which('Xvfb')
    if not xvfb:
        return None
    r, w = os.pipe()
    proc = subprocess.Popen([xvfb, '-displayfd', str(w), '-screen', '0', '1024x768x24', '-nolisten', 'tcp'],
                            pass_fds=[w], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.close(w)
    number = b''
    while True:
        c = os.read(r, 1)
        if not c or c == b'\n':
            break
        number += c
    os.close(r)
    if not number:
        proc.kill()
        return None
    return proc, ':' + number.decode()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--pmx', required=True)
    ap.add_argument('--imgdiff', required=True)
    ap.add_argument('--data', required=True, help='directory with the generated test data')
    ap.add_argument('--reference', required=True)
    ap.add_argument('--output', required=True)
    ap.add_argument('--source', required=True, help='source tree (shaders and toon textures)')
    ap.add_argument('--update', action='store_true', help='write the reference images instead of comparing')
    ap.add_argument('--threshold', default='48')
    ap.add_argument('--max-fraction', default='0.005')
    args = ap.parse_args()

    # meson passes paths relative to the build directory
    for name in ('pmx', 'imgdiff', 'data', 'reference', 'output'):
        setattr(args, name, os.path.abspath(getattr(args, name)))
    os.chdir(args.source)
    os.makedirs(args.output, exist_ok=True)
    env = dict(os.environ)
    env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    xvfb = None
    if not env.get('DISPLAY'):
        xvfb = start_xvfb()
        if xvfb is None:
            print('SKIP: no X display and no Xvfb')
            # CI sets MMP_RENDER_TEST_REQUIRED so that a missing Xvfb fails
            return 1 if os.environ.get('MMP_RENDER_TEST_REQUIRED') else SKIP
        env['DISPLAY'] = xvfb[1]

    failed = []
    try:
        for name, model, motion, extra, frame in CASES:
            out = os.path.join(args.output, name + '.png')
            if os.path.exists(out):
                os.remove(out)
            cmd = [args.pmx, '--size', SIZE, os.path.join(args.data, model)]
            if motion:
                cmd.append(os.path.join(args.data, motion))
            cmd += [x.format(data=args.data) for x in extra]
            cmd += ['--snapshot', str(frame), out]
            res = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 universal_newlines=True, timeout=120)
            if res.returncode != 0 or not os.path.exists(out):
                print('%s: pmx failed (exit %d):\n%s' % (name, res.returncode, res.stdout[-3000:]))
                failed.append(name)
                continue
            ref = os.path.join(args.reference, name + '.png')
            if args.update:
                os.makedirs(args.reference, exist_ok=True)
                shutil.copyfile(out, ref)
                print('%s: updated %s' % (name, ref))
                continue
            if not os.path.exists(ref):
                print('%s: no reference image %s (run with --update)' % (name, ref))
                failed.append(name)
                continue
            diff = os.path.join(args.output, name + '-diff.png')
            res = subprocess.run([args.imgdiff, ref, out, '--diff', diff, '--threshold', args.threshold,
                                  '--max-fraction', args.max_fraction],
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
            print('%s: %s' % (name, res.stdout.strip()))
            if res.returncode != 0:
                print('   expected %s\n   actual   %s\n   diff     %s' % (ref, out, diff))
                failed.append(name)
    finally:
        if xvfb:
            xvfb[0].terminate()
            xvfb[0].wait()

    if failed:
        print('FAILED: ' + ', '.join(failed))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

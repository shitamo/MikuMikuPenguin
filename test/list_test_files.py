#!/usr/bin/env python3
"""Print the loader test inputs (test/regress and the fuzzing seeds), one
path per line relative to test/; used by meson.build."""
import os
import sys

root = os.path.dirname(os.path.abspath(__file__))
for sub in ('regress', os.path.join('fuzz', 'seeds')):
    for dirpath, _, names in os.walk(os.path.join(root, sub)):
        for name in sorted(names):
            if name.startswith('.') or not name.endswith(('.pmx', '.pmd', '.vmd')):
                continue
            print(os.path.relpath(os.path.join(dirpath, name), root))
sys.exit(0)

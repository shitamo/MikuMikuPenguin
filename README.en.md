# LibMikuMikuPenguin(libmmp) v0.2 Readme (English)
Hello~

I wanted to MikuMiku on my favorite operating system, so I wrote a program to do just that.
It's still very buggy, and the API isn't particularly fixed in any manner so it's still too early to effectively use MMP as a library.
However, I hope that this becomes a starting point for the development of MMD on operating systems other than Windows (although you actually can use this library in Windows).

(The above is the original description by the original author, sn0w75. Original code: https://github.com/sn0w75/MikuMikuPenguin/ )

## 2026 modernisation

This tree is based on v0.2.1 with the following changes so that it builds and runs on current Linux:

* GLFW 2 -> GLFW 3, current GLM (0.9.9 / 1.x)
* GL loader GLEW -> libepoxy, image loading SOIL -> stb_image, music via SDL3_mixer (falls back to SDL2_mixer)
* Bone matrices are passed through a texture buffer (no 251-bone limit)
* Fixed crashes with models of more than 32767 vertices, texture index -1, etc.
* Case-insensitive texture lookup
* Playback speed no longer depends on the monitor refresh rate
* Build system changed from autotools to Meson
* Debian packaging (debian/)

Requirements: GLFW 3, libepoxy, GLM, Bullet, stb_image (SDL3_mixer or SDL2_mixer for music), OpenGL 3.2 core profile

## Contents
The contents are listed below:

* COPYING: This license for MikuMikuPenguin is included in here.
* doc: The manual of the original version (HTML).
* include: LibMikuMikuPenguin's header (.h) files are in this folder.
* libmmp: LibMikuMikuPenguin's source (.cpp) files are in this folder.
* shaders: PMXViewer's shaders are in this folder.
* src: PMXViewer's source code (.cpp, .h) are in this folder.
* data: Required textures and other data files are in this folder.
* test: Tests. test/regress holds broken files for the loader regression tests, test/fuzz the fuzz targets.

## Requirements
For LibMikuMikuPenguin, GLFW 3, libepoxy, GLM, Bullet and iconv are needed.

For PMXViewer (PMX and PMD models), libmmp and stb_image (libstb) are needed. The following are optional; the code compiles without them:

* music playback: SDL3_mixer (or SDL2_mixer)
* on-screen controls (GUI): Dear ImGui (libimgui-dev), and fontconfig to find a Japanese font

## Installation
The build uses Meson and Ninja (replaced autotools in the 2026 update):

    $ meson setup build
    $ meson compile -C build
    $ meson test -C build        # tests (see below)

To install libmmp and PMXViewer on your system, run:

    $ sudo meson install -C build

Use `meson setup build -Dsound=disabled -Dgui=disabled` to build without music playback or on-screen controls.
A Debian package can be built with `dpkg-buildpackage -us -uc -b`.

## Tests

`meson test -C build` runs the tests (GitHub Actions also runs them on every push and pull request).
The test model and motions are generated into the build directory by `test/gen_testdata.py` (needs Python 3).

* suite `unit`: PMX / PMD / VMD loading (including broken files), bone, morph, camera and light interpolation, seeking against normal playback, the order of IK and inheritance
* suite `render`: `pmx --snapshot FRAME OUT.png` draws fixed frames, which are compared with the reference images in `test/reference/` (small differences are tolerated).
  Without an X display Xvfb is started; Mesa software rendering (`LIBGL_ALWAYS_SOFTWARE=1`) is used. Without Xvfb the test is skipped.

Run one suite with e.g. `meson test -C build --suite unit`. After an intended change of the rendering, regenerate the reference images with

    $ meson compile -C build update-reference-images

and check `test/reference/*.png` before committing them.
The images and diffs (differing pixels in red) of a failed run are kept in `build/test/render/`.

`meson test` includes loader regression tests that feed broken / hostile PMX, PMD and VMD files to
libmmp (test/regress).  To catch memory errors too, run them in a sanitizer build:

    $ meson setup build-asan -Db_sanitize=address,undefined
    $ meson test -C build-asan

## Fuzzing

The PMX, PMD and VMD loaders have libFuzzer targets (test/fuzz).  After loading, they also play a
motion on the model (morphs, IK, inheritance) and set up and step its physics (OpenGL calls are
replaced by dummy functions).  Build them with clang and `-Dfuzz=true`:

    $ sudo apt install clang libclang-rt-dev     # Debian/Ubuntu
    $ CC=clang CXX=clang++ meson setup build-fuzz -Dfuzz=true -Dgui=disabled -Dsound=disabled
    $ meson compile -C build-fuzz fuzz_pmx fuzz_pmd fuzz_vmd

The seeds are in test/fuzz/seeds (small synthetic files made by `python3 test/fuzz/make_seeds.py`).
Copy them to a corpus directory and run:

    $ mkdir -p corpus/pmx && cp test/fuzz/seeds/pmx/* corpus/pmx/
    $ build-fuzz/test/fuzz/fuzz_pmx corpus/pmx -max_total_time=3600 -timeout=10 \
          -rss_limit_mb=2048 -malloc_limit_mb=512 -max_len=16384

`fuzz_pmd` and `fuzz_vmd` work the same way (seeds in test/fuzz/seeds/pmd and test/fuzz/seeds/vmd).
Add `-fork=N` to use several cores (and `-ignore_crashes=1` to keep going after a crash).  A
reported input (crash-*) is reproduced with `build-fuzz/test/fuzz/fuzz_pmx crash-...`; once fixed,
add a small reproducer to test/regress (`python3 test/regress/make_regress.py` generates the
existing ones; files named `*.reject.*` must fail to load, the others must load).

Load errors are reported with `std::runtime_error`.  Indices are checked when a model is loaded:
optional references (textures, bones, display frames, ...) that point nowhere become "none", and
entries that cannot work (joints without their rigid body, morph offsets of missing vertices, ...)
are dropped.

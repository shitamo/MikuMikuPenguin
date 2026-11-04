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
* MME (MikuMikuEffect)-like screen effects: bloom, glow, depth of field, SSAO, colour correction, FXAA (see "Screen effects" below)

Requirements: GLFW 3, libepoxy, GLM, Bullet, stb_image (SDL3_mixer or SDL2_mixer for music), OpenGL 3.2 core profile

## Screen effects (MME-like)

Looks that MMD users usually get from MME effects are drawn by the viewer itself with OpenGL (no .fx files are read).
With effects on, the scene is first drawn into a floating point offscreen buffer (multisampled like the window, with depth), processed by full-screen passes and then shown.
The on-screen controls are drawn on top and are not affected.

| Effect | Name | What it does | Settings |
|---|---|---|---|
| Bloom | `bloom` | Parts brighter than the threshold bleed into their surroundings | intensity, threshold, spread |
| Glow (AutoLuminous-like) | `glow` | Materials with a specular power of 110 or more emit light (the AutoLuminous convention; higher = brighter). With "bright parts glow too" the brightest parts of the models glow as well | intensity, threshold for the bright parts |
| Depth of field | `dof` | Blurs what is away from the focus distance. By default the focus follows the camera's point of interest (the camera motion's interest point, or the orbit centre of the free camera) | focus distance (manual), strength, max blur |
| SSAO | `ssao` | Darkens creases and occluded places (under the hair, neck, armpits...) | strength, radius (MMD units) |
| Colour correction | `color` | Brightness, contrast, saturation, gamma | the four values |
| FXAA | `fxaa` | Smooths jagged edges | — |

* All effects are off at start.  While they are off the scene is drawn straight into the window as before: same look, same speed.
* The "Effects" section of the settings panel (Settings on the playback bar) turns each effect on and off and adjusts it.
* They can also be given on the command line; `name=value` sets the main strength (bloom/glow intensity, DoF strength, SSAO strength):

      $ pmx --effect bloom,dof miku.pmx dance.vmd -c camera.vmd
      $ pmx --effect bloom=1.2,glow,ssao,fxaa miku.pmx dance.vmd
      $ pmx --dof-focus 30 miku.pmx            # fixed focus distance (turns DoF on)
      $ pmx --color 0.05,1.1,1.2,1.0 miku.pmx  # brightness,contrast,saturation,gamma

* To keep the GPU load down, SSAO, the depth of field blur and bloom are computed at half size or smaller, effects that are off cost nothing, and the glow buffer only exists while glow is on.
* The effect shaders are in `shaders/postfx/` (installed: `/usr/share/mikumikupenguin/shaders/postfx/`).

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

## Video output (AVI output)

Like MMD's "AVI output", the motion can be written frame by frame into a video or numbered images.
Time advances by frames, not by the clock (the physics in 1/60 s steps), and camera / light motion, shadows and several models are drawn as on screen. The on-screen controls are not included.

On the command line, `--render` writes the file without showing a window and quits:

    $ pmx --render out.mp4 -s music.wav -c camera.vmd model.pmx motion.vmd
    $ pmx --render out.mp4 --range 300:899 --size 1920x1080 --fps 60 model.pmx motion.vmd
    $ pmx --render frames/ --range 0:149 model.pmx motion.vmd      # frames/00000.png, 00001.png, ...

* `--render FILE`: `.png` (`out.png` → `out_00000.png`, ...; a format such as `out_%04d.png` works too) or a directory ending in `/` writes numbered PNG files; `.mp4`, `.webm`, `.mkv`, `.mov`, `.avi`, `.gif` ... are encoded by ffmpeg (which must be installed).
* `--range START:END`: VMD frames to write, both included (default: from the beginning to the last key); `300:` or `:899` keep the other end. The length is (END−START+1)/30 seconds.
* `--size WxH`: image size (default 1280x720; rounded down to even numbers for videos).
* `--fps 30|60`: frame rate (default 30).
* With `-s`, the music goes into the video, starting at the beginning of `--range` (`--no-music` leaves it out).
* When the range does not start at frame 0, the motion and the physics are run from frame 0 up to it first, so that the physics looks the same as when played from the beginning.
* No window is shown, but an X display is still needed: on a server, use `xvfb-run pmx --render ...`.

In the window, File → Export video... on the playback bar asks for the range ("From here" takes the current position), size, frame rate, destination and format. While writing, the progress and the frame being written are shown, and Stop cancels (a video file is removed; PNG files written so far are kept).

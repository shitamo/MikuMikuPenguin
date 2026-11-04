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

## Requirements
For LibMikuMikuPenguin, GLFW 3, libepoxy, GLM, Bullet and iconv are needed.

For PMXViewer (PMX and PMD models), libmmp and stb_image (libstb) are needed. The following are optional; the code compiles without them:

* music playback: SDL3_mixer (or SDL2_mixer)
* on-screen controls (GUI): Dear ImGui (libimgui-dev), and fontconfig to find a Japanese font

## Installation
The build uses Meson and Ninja (replaced autotools in the 2026 update):

    $ meson setup build
    $ meson compile -C build
    $ meson test -C build        # unit tests

To install libmmp and PMXViewer on your system, run:

    $ sudo meson install -C build

Use `meson setup build -Dsound=disabled -Dgui=disabled` to build without music playback or on-screen controls.
A Debian package can be built with `dpkg-buildpackage -us -uc -b`.

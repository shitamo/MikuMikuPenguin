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

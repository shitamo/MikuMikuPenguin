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

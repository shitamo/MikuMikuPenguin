#ifndef MMP_GL_H
#define MMP_GL_H

/* OpenGL function loader + windowing.
 * libepoxy resolves GL entry points lazily and works with both GLX and EGL
 * (X11 and Wayland), unlike GLEW 2.x whose GLX build fails under EGL.
 * epoxy must be included before GLFW. */
#include <epoxy/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#endif

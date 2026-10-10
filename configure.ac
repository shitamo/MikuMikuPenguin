#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([MikuMikuPenguin], [0.2.1], [ibenrunnin@gmail.com])
AC_CONFIG_SRCDIR([libmmp/pmx.cpp])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign subdir-objects])
LT_PREREQ([2.2])
LT_INIT([dlopen])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_LANG([C++])
PKG_PROG_PKG_CONFIG

# The code uses C++11 features (u8"" literals compared against std::string,
# char16_t ...).  C++20 changed u8"" to char8_t, so pin the dialect.
AC_MSG_CHECKING([whether $CXX accepts -std=gnu++17])
save_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS -std=gnu++17"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
  [AC_MSG_RESULT([yes]); MMP_CXXSTD="-std=gnu++17"],
  [AC_MSG_RESULT([no]); MMP_CXXSTD="-std=gnu++11"])
CXXFLAGS="$save_CXXFLAGS"
AC_SUBST([MMP_CXXSTD])

# Checks for libraries.
PKG_CHECK_MODULES([GLFW], [glfw3 >= 3.0])
PKG_CHECK_MODULES([GLEW], [glew])
PKG_CHECK_MODULES([GL], [gl])
PKG_CHECK_MODULES([BULLET], [bullet])

# GLM is header-only; it may or may not ship a pkg-config file.
AC_CHECK_HEADER([glm/glm.hpp], [], [AC_MSG_ERROR([GLM (OpenGL Mathematics) headers not found])])

# SOIL has no pkg-config file.
AC_CHECK_HEADER([SOIL/SOIL.h], [], [AC_MSG_ERROR([SOIL headers (SOIL/SOIL.h) not found])])
AC_CHECK_LIB([SOIL], [SOIL_load_image], [SOIL_LIBS="-lSOIL"],
  [AC_MSG_ERROR([libSOIL not found])], [$GL_LIBS])
AC_SUBST([SOIL_LIBS])

AC_ARG_ENABLE([sound],
  [AS_HELP_STRING([--disable-sound], [Disable music playback via SDL2_mixer @<:@default=check@:>@])],
  [:],
  [enable_sound=check])
AS_IF([test "$enable_sound" != "no"],
  [PKG_CHECK_MODULES([SDL2MIXER],
    [sdl2 SDL2_mixer],
    [enable_sound=yes],
    [AS_IF([test "$enable_sound" = "yes"],
      [AC_MSG_ERROR([SDL2_mixer required, but not found.])],
      [enable_sound=no])])])
AM_CONDITIONAL([ENABLE_SOUND], [test "$enable_sound" = "yes"])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h iconv.h])

# files to generate via autotools (.am or .in source files)
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 shaders/Makefile
                 data/share/Makefile
                 include/Makefile
                 libmmp/Makefile
                 doc/Makefile
                 man/Makefile
                 scripts/Makefile
                 test/Makefile])

AC_OUTPUT

AC_MSG_NOTICE([
  MikuMikuPenguin $PACKAGE_VERSION
    C++ standard : $MMP_CXXSTD
    Sound (SDL2_mixer) : $enable_sound
])

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([MikuMikuPenguin], [0.2.1], [ibenrunnin@gmail.com])
AC_CONFIG_SRCDIR([libmmp/pmx.cpp])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
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
PKG_CHECK_MODULES([EPOXY], [epoxy])
PKG_CHECK_MODULES([GL], [gl])
PKG_CHECK_MODULES([BULLET], [bullet])

# GLM is header-only; it may or may not ship a pkg-config file.
AC_CHECK_HEADER([glm/glm.hpp], [], [AC_MSG_ERROR([GLM (OpenGL Mathematics) headers not found])])

# Image loading: stb_image (SOIL bundled a 2008 copy that cannot read
# 16-bit PNGs and is no longer maintained).
PKG_CHECK_MODULES([STB], [stb], [],
  [AC_CHECK_HEADER([stb/stb_image.h], [STB_CFLAGS=""; STB_LIBS="-lstb"],
     [AC_MSG_ERROR([stb_image (libstb-dev) not found])])])

AC_ARG_ENABLE([sound],
  [AS_HELP_STRING([--disable-sound], [Disable music playback via SDL3_mixer/SDL2_mixer @<:@default=check@:>@])],
  [:],
  [enable_sound=check])
sound_backend=none
AS_IF([test "$enable_sound" != "no"],
  [PKG_CHECK_MODULES([SOUND], [sdl3 sdl3-mixer],
    [enable_sound=yes; sound_backend="SDL3_mixer"
     AC_DEFINE([MMP_SDL3_MIXER], [1], [Use SDL3_mixer for music playback])],
    [PKG_CHECK_MODULES([SOUND], [sdl2 SDL2_mixer],
      [enable_sound=yes; sound_backend="SDL2_mixer"],
      [AS_IF([test "$enable_sound" = "yes"],
        [AC_MSG_ERROR([SDL3_mixer or SDL2_mixer required, but not found.])],
        [enable_sound=no])])])])
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
    Sound              : $enable_sound ($sound_backend)
])

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([MikuMikuPenguin], [0.1], [ibenrunnin@gmail.com])
AM_INIT_AUTOMAKE
LT_PREREQ([2.2])
LT_INIT([dlopen])
AC_CONFIG_MACRO_DIR([m4])

AC_ARG_ENABLE([sound],
  [AS_HELP_STRING([--disable-sound], [Enable Sound support @<:@check@:>@])],
  [:],
  [enable_sound=check])
AS_IF([test "$enable_sound" != "no"],
  [PKG_CHECK_MODULES([SDL2MIXER],
    [SDL2_mixer],
    [enable_sound=yes],
    [AS_IF([test "$enable_sound" = "yes"],
      [AC_MSG_ERROR([SDL2_mixer required, but not found.])],
      [enable_sound=no])])])
AM_CONDITIONAL([ENABLE_SOUND], [test "$enable_sound" = "yes"])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT16_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset])

# files to generate via autotools (.am or .in source files)
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([shaders/Makefile])
AC_CONFIG_FILES([data/share/Makefile])
AC_CONFIG_FILES([include/Makefile])
AC_CONFIG_FILES([libmmp/Makefile])
AC_CONFIG_FILES([doc/Makefile])
AC_CONFIG_FILES([man/Makefile])
AC_CONFIG_FILES([scripts/Makefile])

AC_OUTPUT

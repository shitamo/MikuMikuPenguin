bin_PROGRAMS = pmx

pmx_SOURCES = main.cpp pmxvLogger.cpp viewer.cpp viewer.h pmxvLogger.h sound.h
pmx_CPPFLAGS = -DDATA_PATH=\"$(pkgdatadir)\" -I$(top_srcdir)/include \
	$(GLFW_CFLAGS) $(GLEW_CFLAGS) $(GL_CFLAGS) $(BULLET_CFLAGS)
pmx_CXXFLAGS = $(MMP_CXXSTD) $(AM_CXXFLAGS)
pmx_LDADD = ../libmmp/libmmp.la $(SOIL_LIBS) $(GLFW_LIBS) $(GLEW_LIBS) $(GL_LIBS) $(BULLET_LIBS)

if ENABLE_SOUND
pmx_SOURCES += sound.cpp
pmx_CPPFLAGS += $(SDL2MIXER_CFLAGS)
pmx_LDADD += $(SDL2MIXER_LIBS)
else
pmx_SOURCES += dummysound.h dummysound.cpp
endif

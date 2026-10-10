bin_PROGRAMS = pmx

pmx_SOURCES = main.cpp pmxvLogger.cpp viewer.cpp viewer.h pmxvLogger.h sound.h \
	glfw_func_callbacks.cpp glfw_func_callbacks.h
pmx_CPPFLAGS = -DDATA_PATH=\"$(pkgdatadir)\" -I$(top_srcdir)/include $(STB_CFLAGS) \
	$(GLFW_CFLAGS) $(EPOXY_CFLAGS) $(GL_CFLAGS) $(BULLET_CFLAGS)
pmx_CXXFLAGS = $(MMP_CXXSTD) $(AM_CXXFLAGS)
pmx_LDADD = ../libmmp/libmmp.la $(STB_LIBS) $(GLFW_LIBS) $(EPOXY_LIBS) $(GL_LIBS) $(BULLET_LIBS)

if ENABLE_SOUND
pmx_SOURCES += sound.cpp
pmx_CPPFLAGS += $(SOUND_CFLAGS)
pmx_LDADD += $(SOUND_LIBS)
else
pmx_SOURCES += dummysound.h dummysound.cpp
endif

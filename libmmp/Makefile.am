lib_LTLIBRARIES = libmmp.la

libmmp_la_SOURCES = \
	pmx.cpp vmd.cpp \
	motioncontroller.cpp interpolation.cpp \
	bulletphysics.cpp bulletdebugdrawer.cpp mmdphysics.cpp \
	texthandle.cpp shader.cpp glm_helper.cpp glm_helper.h

libmmp_la_CPPFLAGS = -I$(top_srcdir)/include $(GLFW_CFLAGS) $(GLEW_CFLAGS) $(GL_CFLAGS) $(BULLET_CFLAGS)
libmmp_la_CXXFLAGS = $(MMP_CXXSTD) $(AM_CXXFLAGS)
libmmp_la_LIBADD = $(GLFW_LIBS) $(GLEW_LIBS) $(GL_LIBS) $(BULLET_LIBS)
libmmp_la_LDFLAGS = -no-undefined

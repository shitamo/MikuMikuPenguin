TESTS = glm_helper_test
check_PROGRAMS = glm_helper_test
glm_helper_test_SOURCES = glm_helper_test.cpp minunit.h
glm_helper_test_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libmmp $(BULLET_CFLAGS)
glm_helper_test_CXXFLAGS = $(MMP_CXXSTD) $(AM_CXXFLAGS)
glm_helper_test_LDADD = ../libmmp/libmmp.la $(GLFW_LIBS) $(EPOXY_LIBS) $(GL_LIBS) $(BULLET_LIBS)

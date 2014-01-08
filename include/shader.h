#ifndef SHADER_H
#define SHADER_H

#include <iostream>

#include <GL/glew.h>
#define GLFW_INCLUDE_GLU
#include <GL/glfw.h>

const GLchar* ReadShader(const char* filename);
GLuint compileShader(GLenum type, const GLchar *filename);
GLuint loadShaders(std::string vertShaderName, std::string fragShaderName);

#endif

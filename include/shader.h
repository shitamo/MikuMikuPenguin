#ifndef SHADER_H
#define SHADER_H

#include <iostream>

#include "mmp_gl.h"
GLuint compileShaders(std::string vertShaderName, std::string fragShaderName);
void linkShaders(GLuint program);

#endif

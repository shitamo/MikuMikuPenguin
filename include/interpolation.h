#ifndef INTERPOLATION_H
#define INTERPOLATION_H

#include <glm/gtc/matrix_transform.hpp> 
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/euler_angles.hpp>

float bezier(float X, float x1,float y1, float x2,float y2);

glm::fquat Slerp(glm::quat &v0, glm::quat &v1, float alpha);
glm::vec4 Vectorize(const glm::quat theQuat);

glm::vec4 Lerp(const glm::vec4 &start, const glm::vec4 &end, float alpha);
glm::vec4 Slerp(glm::vec4 &v0, glm::vec4 &v1, float alpha);

float Lerp(const float &start, const float &end, float alpha);
float Slerp(float &v0, float &v1, float alpha);

#endif

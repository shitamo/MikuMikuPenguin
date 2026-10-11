#ifndef GLFW_FUNC_CALLBACKS
#define GLFW_FUNC_CALLBACKS

#include "mmp_gl.h"
#include "mmp_glm.h"

extern float theta;
extern float zenith;
extern float radius;
extern float t; //distances to center after rotating up/down

extern float theta2;
extern float radius2;
extern glm::vec3 cameraPosition;
extern glm::vec3 cameraTarget;

extern int prevX,prevY;
extern int prevWheel;

//set every frame by the GUI: the mouse is over / dragging a GUI widget
extern bool guiCapturesMouse;

extern float zoomStep;
extern float minRadius;

//! Recompute cameraPosition (offset from cameraTarget) from theta/zenith/radius
void updateCameraPosition();

//***Mouse Callbacks (GLFW 3)***
void mouseWheel(GLFWwindow *window, double xoffset, double yoffset);
void mouseMotion(GLFWwindow *window, double x, double y);

#endif

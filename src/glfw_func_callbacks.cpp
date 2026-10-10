#include "glfw_func_callbacks.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//Spherical coordinate system (http://en.wikipedia.org/wiki/Spherical_coordinate_system)
float theta=3.0f*M_PI/2.0f;
float zenith=0.0;
float radius=10.0f;
float t=radius; //distances to center after rotating up/down

float theta2=0.0f;
float radius2=20.0f;
glm::vec3 cameraPosition(0.0f, 0.0f, radius*sin(theta));
glm::vec3 cameraTarget(0.0f,0.0f,0.0f);

int prevX=0,prevY=0;
int prevWheel=0;


float zoomStep=1.0f;   //mouse wheel step (set from the model size)
float minRadius=1.0f;

void updateCameraPosition()
{
	t=radius*cos(zenith);
	cameraPosition.y=radius*sin(zenith);
	cameraPosition.x=t*cos(theta);
	cameraPosition.z=t*sin(theta);
}

void mouseWheel(GLFWwindow *window, double xoffset, double yoffset)
{
	(void)window; (void)xoffset;
	if(yoffset>0) radius-=zoomStep; //up: zoom in
	else if(yoffset<0) radius+=zoomStep; //down: zoom out
	if(radius<minRadius) radius=minRadius;
	updateCameraPosition();
}

void mouseMotion(GLFWwindow *window, double xpos, double ypos)
{
	int x=(int)xpos, y=(int)ypos;
	int dx=prevX-x,dy=y-prevY;

	if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS)
	{
		theta+=dx/250.0f;
		zenith+=dy/250.0f;
	
		if(zenith>=M_PI/2) zenith=M_PI/2-0.0001f;
		if(zenith<=-M_PI/2) zenith=-M_PI/2+0.0001f;
		
		updateCameraPosition();
	}
	prevX=x, prevY=y;
}

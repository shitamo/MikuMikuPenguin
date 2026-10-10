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


void mouseWheel(GLFWwindow *window, double xoffset, double yoffset)
{
	(void)window; (void)xoffset;
	if(yoffset>0) //up
	{
		radius-=1.0f;
		if(radius<1.0f) radius=1.0f;
		t=radius*cos(zenith);
		cameraPosition.y=radius*sin(zenith);
	
		cameraPosition.x=t*cos(theta);
		cameraPosition.z=t*sin(theta);
	}
	else if(yoffset<0) //down
	{
		radius+=1.0f;
		t=radius*cos(zenith);
		cameraPosition.y=radius*sin(zenith);
	
		cameraPosition.x=t*cos(theta);
		cameraPosition.z=t*sin(theta);
	}
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
	
		//cout<<theta<<" "<<zenith<<endl;
		//cout<<dx<<" "<<dy<<endl;
	
		t=radius*cos(zenith);
		cameraPosition.y=radius*sin(zenith);
	
		cameraPosition.x=t*cos(theta);
		cameraPosition.z=t*sin(theta);
	
	
		//cameraPosition.z=radius*sin(theta)+radius2*cos(theta2);
	
	}
	prevX=x, prevY=y;
}


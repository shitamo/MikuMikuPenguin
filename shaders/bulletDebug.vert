#version 330 core

//Vertex Properties
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec4 vColor;

out vec4 fColor;

uniform mat4 MVP;

void main()
{
	gl_Position=MVP*vPosition; //MVP * vPosition
	fColor=vColor;
}

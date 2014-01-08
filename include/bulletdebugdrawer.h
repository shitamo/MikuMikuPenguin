#ifndef BULLET_DEBUG_DRAWER_H
#define BULLET_DEBUG_DRAWER_H

#include <GL/glew.h>
#include <GL/glfw.h>
#include "btBulletDynamicsCommon.h"

#include <vector>

#include <glm/gtc/matrix_transform.hpp> 
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>


struct btDebugVertexData
{
	glm::vec4 position;
	glm::vec4 color;
};

//derived class of debugDrawer
class DebugDrawer: public btIDebugDraw
{
	public:
	DebugDrawer(GLuint shaderProgram);
	
	void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color);
	
	
	void drawContactPoint (const btVector3 &PointOnB, const btVector3 &normalOnB, btScalar distance, int lifeTime, const btVector3 &color);
	void reportErrorWarning (const char *warningString);
	void draw3dText(const btVector3 &location, const char *textString);
	void setDebugMode (int debugMode);
	int getDebugMode()const;
	
	void render(); //Push geometry to VAO and render
	
	GLuint shaderProgram;
	GLuint MVPLoc;
	private:
	int m_debugMode;
	
	unsigned int VAO;
	unsigned int VertexArrayBuffer;
	
	std::vector<glm::vec4> lines;
	std::vector<glm::vec4> lineColors;
	
	std::vector<btDebugVertexData> btDebugVertices;
	
	static const float vertices[3][2];
};


#endif

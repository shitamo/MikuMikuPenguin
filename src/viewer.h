#ifndef VIEWER_H
#define VIEWER_H

#include "mmp_gl.h"
#include "mmp_glm.h"

#include <vector>
#include <string>

#define BUFFER_OFFSET(offset) ((void *) (offset))

enum VAO_IDs { Vertices, NumVAOs };
enum Buffer_IDs { VertexArrayBuffer, VertexIndexBuffer, RecordBuffer, NumBuffers };
enum Attrib_IDs { vPosition, vUV, vNormal, vBoneIndices, vBoneWeights, vWeightFormula, vSdefC, vSdefR0, vSdefR1 };
enum Uniform_IDs { 
	uAmbient,uDiffuse,uSpecular,uShininess,
	uIsEdge,uEdgeColor,uEdgeSize,
	uLightDirection,
	uSphereMode,
	uTextureSampler,uSphereSampler,uToonSampler,
	uTextureMul,uTextureAdd,
	uLightColor,uUseToon,uMV,uScreenSize,
	uSphereMul,uSphereAdd,uToonMul,uToonAdd,
	NumUniforms };

namespace ClosedMMDFormat
{
	struct PMXInfo;
	struct VMDInfo;
}
class VMDMotionController;
class BulletPhysics;
class MMDPhysics;
class VertexData;


class Viewer
{
	public:
	Viewer(std::string modelPath, std::string motionPath, std::string musicPath="");
	~Viewer();
	
	void run();
		
	
	private:
	void initGLFW();
	void hackShaderFiles(); //modify #version lines in shader files to match GL version
	void initUniformVarLocations();
	void loadTextures();
	void initBuffers();
	
	void handleEvents();
	void handleLogic();
	void render();
	void fpsCount();
	
	void setCamera(GLuint MVPLoc);
	glm::mat4 viewMatrix, modelViewMatrix;
	bool keyPressed(int key);
	void holdModelInBindPose();
	
	void drawModel(bool drawEdges);
	
	
	GLFWwindow *window;
	int framebufferWidth, framebufferHeight;

	GLuint VAOs[NumVAOs];
	GLuint Buffers[NumBuffers];
	GLuint uniformVars[NumUniforms];

	GLuint MVP_loc;

	ClosedMMDFormat::PMXInfo *pmxInfo;
	ClosedMMDFormat::VMDInfo *vmdInfo;
	VMDMotionController *motionController;

	BulletPhysics *bulletPhysics;
	MMDPhysics *mmdPhysics;

	std::vector<GLuint> textures;
	GLuint whiteTexture; //bound when a material has no texture
	GLuint textureOrWhite(int index);
	
	//The OpenGL Version hints used that successfully opened the window
	int GLVersionHintMajor,GLVersionHintMinor;
	//The actual OpenGL Version recieved
	int GLVersionMajor,GLVersionMinor,GLVersionRevision;
	
	std::string vertShaderPath,fragShaderPath;
	GLuint shaderProgram;
	std::string bulletVertPath,bulletFragPath;
	
	//***Timing Variables
	double startTime;
	int ticks;
	
	glm::vec3 modelTranslate;
};


#endif

#ifndef VIEWER_H
#define VIEWER_H

#include "mmp_gl.h"
#include "mmp_glm.h"

#include <vector>
#include <string>

#define BUFFER_OFFSET(offset) ((void *) (offset))

enum VAO_IDs { Vertices, IKDebugVertices, RigidDebugVertices, NumVAOs };
enum Buffer_IDs { VertexArrayBuffer, VertexIndexBuffer, IKVertexArrayBuffer, RecordBuffer, NumBuffers };
enum Attrib_IDs { vPosition, vUV, vNormal, vBoneIndices, vBoneWeights, vWeightFormula };
enum Uniform_IDs { 
	uAmbient,uDiffuse,uSpecular,uShininess,
	uIsEdge,uEdgeColor,uEdgeSize,
	uHalfVector,uLightDirection,
	uSphereMode,
	uTextureSampler,uSphereSampler,uToonSampler,
	NumUniforms };

struct PMXInfo;
struct VMDInfo;
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
	void initUniformVarLocations();
	void loadTextures();
	void initBuffers();
	
	void handleEvents();
	void handleLogic();
	void render();
	
	void setCamera(GLuint MVPLoc);
	bool keyPressed(int key);
	void holdModelInBindPose();
	
	void drawModel(bool drawEdges);
	void drawIKMarkers();
	
	
	GLFWwindow *window;
	int framebufferWidth, framebufferHeight;

	GLuint VAOs[NumVAOs];
	GLuint Buffers[NumBuffers];
	GLuint uniformVars[NumUniforms];

	GLuint MVP_loc;

	PMXInfo *pmxInfo;
	VMDInfo *vmdInfo;
	VMDMotionController *motionController;

	BulletPhysics *bulletPhysics;
	MMDPhysics *mmdPhysics;

	std::vector<GLuint> textures;
	GLuint whiteTexture; //bound when a material has no texture
	GLuint textureOrWhite(int index);
	
	GLuint shaderProgram;
	
	VertexData *IKVertexData;
	VertexData *RigidVertexData;
	
	
	//***Timing Variables
	double startTime;
	int ticks;
	
	glm::vec3 modelTranslate;
};


#endif

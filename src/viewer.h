#ifndef VIEWER_H
#define VIEWER_H

#include "mmp_gl.h"
#include "mmp_glm.h"
#include "cameracontroller.h"
#include "lightcontroller.h"

#include <vector>
#include <string>

#define BUFFER_OFFSET(offset) ((void *) (offset))

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
	uRenderMode,uGroundShadowColor,uSelfShadow,uShadowMap,uShadowTexelSize,uShadowMVP,
	NumUniforms };

//Texture unit of the self-shadow depth map (0-2 material textures, 3 bones)
#define SHADOW_TEXTURE_UNIT 4

namespace ClosedMMDFormat
{
	struct PMXInfo;
	struct VMDInfo;
}
class VMDMotionController;
class BulletPhysics;
class MMDPhysics;
class VertexData;

//! A model to load: PMX/PMD file, its motion (may be empty) and a position
struct ModelSpec
{
	std::string modelPath;
	std::string motionPath;
	glm::vec3 offset; //!< position in OpenGL coordinates
	ModelSpec(const std::string &m="", const std::string &v=""): modelPath(m), motionPath(v), offset(0.0f) {}
};

//! One model of the scene with its own motion, physics world and GL buffers
struct SceneModel
{
	ModelSpec spec;
	ClosedMMDFormat::PMXInfo *pmxInfo;
	ClosedMMDFormat::VMDInfo *vmdInfo;
	VMDMotionController *motionController;
	BulletPhysics *bulletPhysics;
	MMDPhysics *mmdPhysics;
	std::vector<GLuint> textures; //the model's textures (shared toons are in Viewer::sharedToon)
	GLuint vao, vertexBuffer, indexBuffer;
	glm::vec3 restMin, restMax;   //bounding box of the mesh at rest (model space)
	glm::vec3 offset;             //position in the scene (editable in the GUI)
	bool visible;
};


class Viewer
{
	public:
	Viewer(const std::vector<ModelSpec> &modelSpecs, std::string musicPath="", std::string cameraPath="", std::string lightPath="");
	~Viewer();
	
	void run();
	void setLoop(bool loop) { loopPlayback=loop; } //!< restart from the beginning at the end of the motion
	void setGUI(bool enable) { guiVisible=enable; } //!< show the on-screen controls
		
	
	private:
	void initGLFW();
	void hackShaderFiles(); //modify #version lines in shader files to match GL version
	void bindAttribLocations(); //before linking the shader program
	void initUniformVarLocations();
	void loadSharedTextures();
	void loadTextures(SceneModel &m);
	void initBuffers(SceneModel &m);
	
	void handleEvents();
	void handleLogic();
	void render();
	void fpsCount();
	
	void setCamera(); //!< compute projectionMatrix / viewMatrix for this frame
	void resetCamera(); //frame all models (MMD-like front view)
	void updateSceneBounds();
	VMDCameraController cameraController;
	VMDLightController lightController;
	bool useLightMotion;
	bool useCameraMotion;
	float lastTheta, lastZenith, lastRadius; //to detect manual camera moves
	glm::vec3 modelMin, modelMax; //bounds of the whole scene at rest
	glm::mat4 viewMatrix;
	bool keyPressed(int key);
	void handleKey(int key, int mods, bool repeat); //!< key press (or auto-repeat) from the GLFW key callback
	
	//***Models
	std::vector<SceneModel*> models;
	int currentTime() const; //!< playback time in 1/60 s steps (all models are kept in step)
	glm::mat4 modelMatrixFor(const SceneModel &m) const;
	bool modelShown(const SceneModel &m) const;
	
	//***Playback control
	bool paused;
	bool loopPlayback;
	int endTime;        //!< end of the motion in 1/60 s steps (last key)
	void seekTo(int t); //!< jump to time t (1/60 s steps) and reset the physics to that pose
	void stepLogic();   //!< advance the animation and physics by one 1/60 s step
	void setPaused(bool p);
	void stepFrame(int direction); //!< one VMD frame forward (+1) or back (-1), pausing playback
	
	//***Display options (keys and GUI)
	bool physicsEnabled;
	bool showModel;
	int debugDrawMode; //0 none, 1 wireframe, 2 AABB, 3 constraints, 4 constraint limits
	std::string cameraPathShown, lightPathShown, musicPathShown;
	
	//***GUI (Dear ImGui; stubs when built without it)
	bool guiAvailable, guiVisible;
	bool guiShowSettings;
	void initGUI();
	void guiFrame();   //!< build the GUI for this frame (may change the playback state)
	void guiRender();  //!< draw the GUI on top of the scene
	void shutdownGUI();
	void updateCameraUniforms();
	
	void drawModel(SceneModel &m, bool drawEdges);
	
	//***Shadows
	bool useGroundShadow, useSelfShadow, hasStencil;
	GLuint shadowFBO, shadowTexture;
	int shadowMapSize;
	glm::mat4 projectionMatrix; //set by setCamera()
	std::vector<std::pair<unsigned,bool>> selfShadowKeys; //VMD self shadow on/off (frame, on)
	void initShadowMap();
	MMDLightState currentLight();
	bool selfShadowActive();
	glm::mat4 lightViewProjection(const glm::vec3 &lightDir); //world -> light clip space
	void drawShadowCasters(SceneModel &m, int renderMode); //geometry only, for the shadow passes
	void renderShadowMap(const glm::mat4 &lightVP);
	void drawGroundShadow(const glm::vec3 &lightDir);
	
	
	GLFWwindow *window;
	int framebufferWidth, framebufferHeight;

	GLuint uniformVars[NumUniforms];

	GLuint MVP_loc;

	GLuint whiteTexture; //bound when a material has no texture
	GLuint sharedToon[10]; //toon01.bmp - toon10.bmp
	GLuint textureOrWhite(const SceneModel &m, int index);
	
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
	
	glm::vec3 modelTranslate; //!< Up/Down keys: moves all models (free camera)
};


#endif

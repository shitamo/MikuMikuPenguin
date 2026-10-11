#include "viewer.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <stb/stb_image.h>

#include <cstdio>
#include <cstddef>
#include <cstring>
#include <strings.h>
#include <unistd.h>
#include <dirent.h>
//#include <FreeImage.h>

#include "texthandle.h"
#include "pmx.h"
#include "vmd.h"
#include "shader.h"

#include "motioncontroller.h"
#include "bulletphysics.h"
#include "mmdphysics.h"

#include "sound.h"
#include <algorithm>

static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void dropCallback(GLFWwindow *window, int count, const char **paths);
static std::vector<std::string> droppedFiles;

#include "glfw_func_callbacks.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//#define MODELDUMP true

using namespace std;
using namespace ClosedMMDFormat;


//"dir/model.pmx" -> folder "dir", file "/model.pmx" (as readPMX expects)
static void splitModelPath(const string &path, string &folder, string &file)
{
	size_t index=path.rfind("/");
	if(index==string::npos)
	{
		folder="";
		file=path;
	}
	else
	{
		file=path.substr(index);
		folder=path.substr(0,index);
	}
}

Viewer::Viewer(const vector<ModelSpec> &modelSpecs, string musicPath, string cameraPath, string lightPath)
{
	musicPathShown=musicPath;
	cameraPathShown=cameraPath;
	lightPathShown=lightPath;
	cameraFileInfo=NULL;
	lightFileInfo=NULL;
	physicsEnabled=true;
	showModel=true;
	debugDrawMode=0;
	guiAvailable=false;
	guiVisible=true;
	guiShowSettings=false;
	sceneTime=0;
	paused=false;
	loopPlayback=false;
	endTime=0;
	useCameraMotion=false;
	useLightMotion=false;
	
	//***Read the files given on the command line (before opening the window,
	//so that errors appear quickly; a broken file is fatal here)
	vector<SceneModel*> loaded;
	for(const ModelSpec &spec : modelSpecs) loaded.push_back(readSceneModel(spec));
	if(!cameraPath.empty()) cameraFileInfo=&readVMD(cameraPath);
	if(!lightPath.empty()) lightFileInfo=&readVMD(lightPath);
	
	initGLFW();
	
	bindAttribLocations();
	linkShaders(shaderProgram);
	glUseProgram(shaderProgram);
	
	initUniformVarLocations();
	initShadowMap();
	initGUI();
	
	MVP_loc = glGetUniformLocation(shaderProgram, "MVP");
	
	//Set OpenGL render settings
	glDisable(GL_CULL_FACE);
	//PMX triangles are clockwise in Direct3D; mirroring Z for OpenGL keeps them
	//clockwise on screen, so clockwise is the front face here.
	glFrontFace(GL_CW);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPointSize(5.0);
	glClearColor(1,1,1,1);
	
	loadSharedTextures();
	
	for(SceneModel *m : loaded)
	{
		setupSceneModel(*m);
		models.push_back(m);
	}
	
	initSound(musicPath);
	
	//Initialize timer variables
	startTime = glfwGetTime();
	ticks=0;
	
	modelTranslate=glm::vec3(0.0f);
	updateSceneBounds();
	resetCamera();
	
	//Shadows (MMD defaults: ground shadow and self shadow on)
	useGroundShadow=true;
	useSelfShadow=true;
	
	rebuildSceneTracks();
	if(useCameraMotion) cout<<"Using camera motion (press C to toggle)"<<endl;
	if(useLightMotion) cout<<"Using light motion (press L to toggle)"<<endl;
	lastTheta=theta; lastZenith=zenith; lastRadius=radius;
}

//Read a model and its motion (throws std::exception on a broken file)
SceneModel *Viewer::readSceneModel(const ModelSpec &spec)
{
	SceneModel *m=new SceneModel();
	m->spec=spec;
	string folder, file;
	splitModelPath(spec.modelPath, folder, file);
	m->pmxInfo=&readPMX(folder, file);
	m->vmdInfo=spec.motionPath.empty() ? new ClosedMMDFormat::VMDInfo() : &readVMD(spec.motionPath);
	m->offset=spec.offset;
	m->visible=true;
	m->motionController=NULL;
	m->bulletPhysics=NULL;
	m->mmdPhysics=NULL;
	m->vao=m->vertexBuffer=m->indexBuffer=0;
	return m;
}

//Textures, buffers, motion controller and physics of a model, posed at the
//current time.  Every model has its own Bullet world (models do not collide
//with each other).
void Viewer::setupSceneModel(SceneModel &m)
{
	loadTextures(m);
	initBuffers(m);
	
	glBindBuffer(GL_ARRAY_BUFFER, m.vertexBuffer); //the controller streams vertices into the bound buffer
	m.motionController=new VMDMotionController(*m.pmxInfo, *m.vmdInfo, shaderProgram);
	
	m.bulletPhysics=new BulletPhysics(bulletVertPath, bulletFragPath);
	glUseProgram(shaderProgram); //restore after BulletPhysics compiled its debug shader
	m.mmdPhysics=new MMDPhysics(*m.pmxInfo, m.motionController, m.bulletPhysics);
	
	m.motionController->setTime(sceneTime);
	m.motionController->updateVertexMorphs();
	m.motionController->updateBoneAnimation();
	//Start the physics from the current pose instead of the bind pose
	m.mmdPhysics->resetToCurrentPose();
	m.motionController->updateBoneMatrix();
	
	//mesh bounding box at rest, used to frame the camera
	m.restMin=glm::vec3(1e30f); m.restMax=glm::vec3(-1e30f);
	for(int i=0; i<m.pmxInfo->vertex_continuing_datasets; ++i)
	{
		m.restMin=glm::min(m.restMin, m.pmxInfo->vertices[i]->pos);
		m.restMax=glm::max(m.restMax, m.pmxInfo->vertices[i]->pos);
	}
	if(m.pmxInfo->vertex_continuing_datasets==0) { m.restMin=glm::vec3(-10.0f,0.0f,-10.0f); m.restMax=glm::vec3(10.0f,20.0f,10.0f); }
}

void Viewer::destroySceneModel(SceneModel *m)
{
	if(!m) return;
	delete m->mmdPhysics;
	delete m->bulletPhysics;
	delete m->motionController;
	if(!m->textures.empty()) glDeleteTextures((GLsizei)m->textures.size(), m->textures.data());
	if(m->vertexBuffer) glDeleteBuffers(1, &m->vertexBuffer);
	if(m->indexBuffer) glDeleteBuffers(1, &m->indexBuffer);
	if(m->vao) glDeleteVertexArrays(1, &m->vao);
	//PMXInfo / VMDInfo own raw pointers without destructors; they are leaked
	//(a few MB per removed model)
	delete m;
	glUseProgram(shaderProgram);
}

//Camera, light and self shadow keys: from the camera (-c) / light (-l) VMD,
//otherwise from the first motion VMD that has them (MMD saves camera and
//light together, so the camera VMD also provides the light).  Also the end
//of the motion.  Called whenever files are added or removed.
void Viewer::rebuildSceneTracks()
{
	bool hadCamera=!cameraController.empty(), hadLight=!lightController.empty();
	auto firstMotionWith=[&](auto pred)->ClosedMMDFormat::VMDInfo*
	{
		for(SceneModel *m : models) if(pred(*m->vmdInfo)) return m->vmdInfo;
		return NULL;
	};
	ClosedMMDFormat::VMDInfo *src;
	src=cameraFileInfo ? cameraFileInfo : firstMotionWith([](const ClosedMMDFormat::VMDInfo &v){ return !v.cameraFrames.empty(); });
	cameraController=src ? VMDCameraController(src->cameraFrames) : VMDCameraController();
	src=lightFileInfo;
	if(!src && cameraFileInfo && !cameraFileInfo->lightFrames.empty()) src=cameraFileInfo;
	if(!src) src=firstMotionWith([](const ClosedMMDFormat::VMDInfo &v){ return !v.lightFrames.empty(); });
	lightController=src ? VMDLightController(src->lightFrames) : VMDLightController();
	
	//turn camera / light motion on when they appear, off when they go away
	if(cameraController.empty()) useCameraMotion=false;
	else if(!hadCamera) useCameraMotion=true;
	if(lightController.empty()) useLightMotion=false;
	else if(!hadLight) useLightMotion=true;
	
	selfShadowKeys.clear();
	{
		src=(cameraFileInfo && !cameraFileInfo->selfShadowFrames.empty()) ? cameraFileInfo
		    : firstMotionWith([](const ClosedMMDFormat::VMDInfo &v){ return !v.selfShadowFrames.empty(); });
		std::vector<ClosedMMDFormat::VMDSelfShadowFrame> frames;
		if(src) frames=src->selfShadowFrames;
		std::stable_sort(frames.begin(), frames.end(),
			[](const ClosedMMDFormat::VMDSelfShadowFrame &a, const ClosedMMDFormat::VMDSelfShadowFrame &b){ return a.frame<b.frame; });
		for(const auto &f : frames) selfShadowKeys.push_back(std::make_pair(f.frame, f.type!=0));
	}
	
	//the motion ends at its last key (any model, camera or light)
	unsigned last=0;
	for(SceneModel *m : models) last=std::max(last, m->motionController->lastKeyFrame());
	last=std::max(last, cameraController.lastFrame());
	last=std::max(last, lightController.lastFrame());
	endTime=(int)last*2;
}

//***Loading files while running (GUI, drag and drop).  Errors are reported
//in loadError instead of terminating the viewer.
bool Viewer::addModel(const ModelSpec &spec)
{
	SceneModel *m=NULL;
	try
	{
		m=readSceneModel(spec);
	}
	catch(const std::exception &e)
	{
		loadError=e.what();
		return false;
	}
	bool first=models.empty();
	setupSceneModel(*m);
	models.push_back(m);
	updateSceneBounds();
	if(first) resetCamera();
	rebuildSceneTracks();
	return true;
}

void Viewer::removeModel(size_t index)
{
	if(index>=models.size()) return;
	destroySceneModel(models[index]);
	models.erase(models.begin()+index);
	updateSceneBounds();
	rebuildSceneTracks();
}

//Give model `index` another motion (the model is reloaded so that its
//controller and physics start clean); an empty path removes the motion
bool Viewer::setModelMotion(size_t index, const string &motionPath)
{
	if(index>=models.size()) return false;
	SceneModel *old=models[index];
	ModelSpec spec=old->spec;
	spec.motionPath=motionPath;
	spec.offset=old->offset;
	SceneModel *m=NULL;
	try
	{
		m=readSceneModel(spec);
	}
	catch(const std::exception &e)
	{
		loadError=e.what();
		return false;
	}
	m->visible=old->visible;
	destroySceneModel(old);
	setupSceneModel(*m);
	models[index]=m;
	rebuildSceneTracks();
	return true;
}

bool Viewer::setCameraFile(const string &path)
{
	ClosedMMDFormat::VMDInfo *info=NULL;
	if(!path.empty())
	{
		try { info=&readVMD(path); }
		catch(const std::exception &e) { loadError=e.what(); return false; }
		if(info->cameraFrames.empty()) loadError=string("No camera keys in ")+path;
	}
	cameraFileInfo=info;
	cameraPathShown=path;
	cameraController=VMDCameraController(); //re-enable camera motion if keys are found
	rebuildSceneTracks();
	return true;
}

bool Viewer::setLightFile(const string &path)
{
	ClosedMMDFormat::VMDInfo *info=NULL;
	if(!path.empty())
	{
		try { info=&readVMD(path); }
		catch(const std::exception &e) { loadError=e.what(); return false; }
		if(info->lightFrames.empty()) loadError=string("No light keys in ")+path;
	}
	lightFileInfo=info;
	lightPathShown=path;
	lightController=VMDLightController();
	rebuildSceneTracks();
	return true;
}

bool Viewer::setMusicFile(const string &path)
{
	if(!path.empty() && !soundLoad(path))
	{
		loadError=string("Cannot play ")+path;
		return false;
	}
	if(path.empty()) soundLoad("");
	musicPathShown=path;
	soundSeek(sceneTime/60.0, paused);
	return true;
}

//Decide what a dropped / chosen file is: a model, a motion (for the model
//without one, else the selected model), a camera/light VMD (no bone or
//morph keys), or music
void Viewer::openFileAuto(const string &path, int targetModel)
{
	string lower=path;
	for(char &c : lower) c=tolower((unsigned char)c);
	auto ends=[&](const char *ext){ size_t n=strlen(ext); return lower.size()>=n && lower.compare(lower.size()-n, n, ext)==0; };
	
	if(ends(".pmx") || ends(".pmd"))
	{
		addModel(ModelSpec(path));
	}
	else if(ends(".vmd"))
	{
		ClosedMMDFormat::VMDInfo *info=NULL;
		try { info=&readVMD(path); }
		catch(const std::exception &e) { loadError=e.what(); return; }
		bool motion=!info->boneFrames.empty() || !info->morphFrames.empty();
		if(!motion && !info->cameraFrames.empty()) setCameraFile(path);
		else if(!motion && !info->lightFrames.empty()) setLightFile(path);
		else if(models.empty()) loadError=string("Load a model first for the motion ")+path;
		else
		{
			int target=targetModel;
			if(target<0 || target>=(int)models.size())
			{
				target=(int)models.size()-1;
				for(size_t i=0; i<models.size(); ++i)
				{
					if(models[i]->spec.motionPath.empty()) { target=(int)i; break; }
				}
			}
			setModelMotion(target, path);
		}
	}
	else
	{
		setMusicFile(path);
	}
}

void Viewer::processPendingLoads()
{
	std::vector<PendingLoad> loads;
	loads.swap(pendingLoads);
	for(const PendingLoad &l : loads)
	{
		switch(l.kind)
		{
			case PendingLoad::Auto:   openFileAuto(l.path, l.model); break;
			case PendingLoad::Model:  addModel(ModelSpec(l.path)); break;
			case PendingLoad::Motion: setModelMotion(l.model, l.path); break;
			case PendingLoad::Camera: setCameraFile(l.path); break;
			case PendingLoad::Light:  setLightFile(l.path); break;
			case PendingLoad::Music:  setMusicFile(l.path); break;
			case PendingLoad::Remove: removeModel(l.model); break;
		}
	}
}

int Viewer::currentTime() const
{
	return sceneTime;
}

glm::mat4 Viewer::modelMatrixFor(const SceneModel &m) const
{
	//Up/Down (modelTranslate) only moves the models with the free camera
	return glm::translate(m.offset + (useCameraMotion ? glm::vec3(0.0f) : modelTranslate));
}

bool Viewer::modelShown(const SceneModel &m) const
{
	//A hides the models; so does the VMD "表示" (show) flag
	return showModel && m.visible && (guiCapturesKeyboard || glfwGetKey(window, GLFW_KEY_A)!=GLFW_PRESS) && m.motionController->isModelVisible();
}

void Viewer::updateSceneBounds()
{
	modelMin=glm::vec3(1e30f); modelMax=glm::vec3(-1e30f);
	for(SceneModel *m : models)
	{
		modelMin=glm::min(modelMin, m->restMin+m->offset);
		modelMax=glm::max(modelMax, m->restMax+m->offset);
	}
	if(models.empty()) { modelMin=glm::vec3(-10.0f,0.0f,-10.0f); modelMax=glm::vec3(10.0f,20.0f,10.0f); }
}

void Viewer::handleLogic()
{
	if(!paused)
	{
		if(loopPlayback && endTime>0 && currentTime()>=endTime)
		{
			seekTo(0);
		}
		else
		{
			stepLogic();
		}
	}
	updateCameraUniforms();
}

void Viewer::stepLogic()
{
	bool doPhysics=physicsEnabled && !keyPressed(GLFW_KEY_Q);
	
	//nothing to play (no keys yet): only the physics runs, so that a motion
	//loaded later starts from its beginning
	bool advance=endTime>0;
	if(advance) ++sceneTime;
	for(SceneModel *m : models)
	{
		if (advance && !m->motionController->advanceTime())
		{
			m->motionController->updateVertexMorphs();
			m->motionController->updateBoneAnimation();
		}
		m->mmdPhysics->updateBones(doPhysics);
		m->motionController->updateBoneMatrix();
	}
}

void Viewer::updateCameraUniforms()
{
	setCamera();
}

void Viewer::seekTo(int t)
{
	if(t<0) t=0;
	if(endTime>0 && t>endTime) t=endTime;
	sceneTime=t;
	for(SceneModel *m : models)
	{
		m->motionController->setTime(t);
		m->motionController->updateVertexMorphs();
		m->motionController->updateBoneAnimation();
		//physics cannot be run backwards: restart it from the new pose
		m->mmdPhysics->resetToCurrentPose();
		m->motionController->updateBoneMatrix();
	}
	soundSeek(t/60.0, paused);
	updateCameraUniforms();
}

void Viewer::render()
{
	glUseProgram(shaderProgram);
	bool anyShown=false;
	for(SceneModel *m : models) anyShown = anyShown || modelShown(*m);
	if(anyShown)
	{
		MMDLightState light=currentLight();
		glm::mat4 lightVP(1.0f);
		bool selfShadow=selfShadowActive();
		if(selfShadow)
		{
			lightVP=lightViewProjection(light.direction);
			renderShadowMap(lightVP);
		}
		if(useGroundShadow) drawGroundShadow(light.direction);
		
		//clip space [-1,1] -> texture space [0,1]
		glm::mat4 bias=glm::translate(glm::vec3(0.5f))*glm::scale(glm::vec3(0.5f));
		for(SceneModel *m : models)
		{
			if(!modelShown(*m)) continue;
			glm::mat4 M=modelMatrixFor(*m);
			glm::mat4 shadowMVP=selfShadow ? bias*lightVP*M : glm::mat4(1.0f);
			glUniformMatrix4fv(uniformVars[uShadowMVP],1,GL_FALSE,&shadowMVP[0][0]);
			drawModel(*m, false); //draw model
			drawModel(*m, true); //draw outlines (MMD draws them after the model, depth tested)
		}
		glDisable(GL_CULL_FACE);
	}
	
	//Bullet debug drawing: selected in the GUI, or while S/D/F/G is held
	int debugMode=debugDrawMode;
	if(keyPressed(GLFW_KEY_S)) debugMode=1;
	else if(keyPressed(GLFW_KEY_D)) debugMode=2;
	else if(keyPressed(GLFW_KEY_F)) debugMode=3;
	else if(keyPressed(GLFW_KEY_G)) debugMode=4;
	static const int bulletModes[]={0, btIDebugDraw::DBG_DrawWireframe, btIDebugDraw::DBG_DrawAabb,
	                                btIDebugDraw::DBG_DrawConstraints, btIDebugDraw::DBG_DrawConstraintLimits};
	if(debugMode>0 && debugMode<5)
	{
		for(SceneModel *m : models)
		{
			glUseProgram(m->bulletPhysics->debugDrawer->shaderProgram);
			glm::mat4 MVP=projectionMatrix*viewMatrix*modelMatrixFor(*m);
			glUniformMatrix4fv(m->bulletPhysics->debugDrawer->MVPLoc, 1, GL_FALSE, &MVP[0][0]);
			m->bulletPhysics->SetDebugMode(bulletModes[debugMode]);
			m->bulletPhysics->DebugDrawWorld();
		}
	}
	glUseProgram(shaderProgram); //Restore shader program after drawing Bullet debug
}

void Viewer::fpsCount()
{
	static double last_time;
	static int nb_frames[] = {0,0,0,0};
	static const int nbf_fps = sizeof(nb_frames) / sizeof(int);
	static const double seq = 1.0 / double(nbf_fps);
	static int nbf_count = 0;
	static const char* window_name = "PMX Viewer";
	if (!last_time) last_time = glfwGetTime();
	
	double current_time = glfwGetTime();
	for (int i = 0; i < nbf_fps; i++)
	{
		nb_frames[i]++;
	}

	if (current_time - last_time >= seq)
	{
		int nbf_now = nbf_count % nbf_fps;
		char title[128];
		snprintf(title, sizeof(title), "%s | %2d fps | frame %d / %d%s%s", window_name, nb_frames[nbf_now],
		         std::min(currentTime(), std::max(endTime, 0))/2, endTime/2, paused ? " | paused" : "", loopPlayback ? " | loop" : "");
		glfwSetWindowTitle(window, title);
		nb_frames[nbf_now] = 0;
		last_time += seq;
		nbf_count++;
	}
}

void Viewer::run()
{
	//The animation and the physics advance in fixed 1/60 s steps.  Step them
	//according to the elapsed wall-clock time so that playback speed does
	//not depend on the monitor refresh rate (e.g. 120/144 Hz with vsync) or
	//on a slow renderer.
	static const double STEP=1.0/60.0;
	static const int MAX_STEPS_PER_FRAME=4;
	double lastTime=glfwGetTime();
	double accumulator=STEP; //run one logic step before the first frame
	
	while(!keyPressed(GLFW_KEY_ESCAPE) && !glfwWindowShouldClose(window))
	{
		glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
		glViewport(0, 0, framebufferWidth, framebufferHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		
		double now=glfwGetTime();
		accumulator+=now-lastTime;
		lastTime=now;
		startTime=now;
		
		fpsCount();
		handleEvents();
		
		int steps=0;
		while(accumulator>=STEP && steps<MAX_STEPS_PER_FRAME)
		{
			handleLogic();
			accumulator-=STEP;
			++steps;
		}
		if(steps==MAX_STEPS_PER_FRAME) accumulator=0.0; //too slow to keep up: drop time instead of spiralling
		if(steps==0) //still update the camera so mouse movement stays smooth
		{
			updateCameraUniforms();
		}
		render();
		guiRender();
		
		glfwSwapBuffers(window);
	}
}

void Viewer::setCamera()
{
	float aspect = (framebufferWidth>0 && framebufferHeight>0) ? (float)framebufferWidth/(float)framebufferHeight : 16.0f/9.0f;
	if(useCameraMotion)
	{
		MMDCameraState cam=cameraController.evaluate(currentTime()/2.0f);
		if(cam.perspective)
		{
			projectionMatrix = glm::perspective(cam.fov, aspect, 0.5f, 5000.0f);
		}
		else //MMD "perspective OFF": orthographic view of the same size at the interest point
		{
			float h = std::abs(cam.distance) * tanf(cam.fov*0.5f);
			if(h < 0.01f) h = 0.01f;
			projectionMatrix = glm::ortho(-h*aspect, h*aspect, -h, h, -5000.0f, 5000.0f);
		}
		viewMatrix = cam.viewMatrix();
		return;
	}
	
	//MMD's default field of view is 30 degrees
	float nearPlane = glm::max(radius*0.01f, 0.01f);
	float farPlane  = radius*20.0f + glm::length(modelMax-modelMin)*4.0f;
	projectionMatrix = glm::perspective(glm::radians(30.0f), aspect, nearPlane, farPlane);
	// Camera matrix: orbit around cameraTarget
	viewMatrix = glm::lookAt(
		cameraTarget + glm::vec3(cameraPosition.x,cameraPosition.y,-cameraPosition.z),
		cameraTarget,
		glm::vec3(0,1,0)
	);
}

void Viewer::resetCamera()
{
	//Look at the centre of the model from the front (MMD looks from -Z in its
	//own coordinates, i.e. +Z here) and back off until the whole model fits
	//vertically and horizontally with some margin.
	glm::vec3 size=modelMax-modelMin;
	glm::vec3 centre=(modelMin+modelMax)*0.5f;
	float aspect = (framebufferWidth>0 && framebufferHeight>0) ? (float)framebufferWidth/(float)framebufferHeight : 16.0f/9.0f;
	float halfFov=glm::radians(30.0f)*0.5f;
	float fitHeight=(size.y*0.5f)/tanf(halfFov);
	float fitWidth =(glm::max(size.x, size.z)*0.5f)/(tanf(halfFov)*aspect);
	float dist=glm::max(fitHeight, fitWidth)*1.15f + size.z*0.5f;
	if(!(dist>0.0f) || dist>1e6f) dist=45.0f;
	
	//a single model is framed on the Y axis, several around their common centre
	cameraTarget=(models.size()>1) ? centre : glm::vec3(0.0f, centre.y, 0.0f);
	radius=dist;
	theta=3.0f*M_PI/2.0f;
	zenith=0.0f;
	zoomStep=dist*0.05f;
	minRadius=dist*0.02f;
	updateCameraPosition();
	modelTranslate=glm::vec3(0.0f);
}

void Viewer::drawModel(SceneModel &m, bool drawEdges)
{
	ClosedMMDFormat::PMXInfo *pmxInfo=m.pmxInfo;
	VMDMotionController *motionController=m.motionController;
	//Bind VAO and related Buffers
	glBindVertexArray(m.vao);
	glBindBuffer(GL_ARRAY_BUFFER, m.vertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.indexBuffer);
	motionController->bindBoneTexture();
	glm::mat4 M=modelMatrixFor(m);
	glm::mat4 modelViewMatrix=viewMatrix*M;
	glm::mat4 MVP=projectionMatrix*modelViewMatrix;
	glUniformMatrix4fv(MVP_loc, 1, GL_FALSE, &MVP[0][0]);
	
	if(drawEdges)
	{
		glDisable(GL_BLEND);
		glCullFace(GL_FRONT);
        glUniform1i(uniformVars[uIsEdge], 1);
        
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
	}
	else
	{        
        glEnable(GL_BLEND);
		glCullFace(GL_BACK);
		glUniform1i(uniformVars[uIsEdge], 0);
		
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
	}
	
	
	//MMD default light: colour (154,154,154), direction (-0.5,-1.0,0.5) in MMD
	//(left-handed) coordinates = (-0.5,-1.0,-0.5) here.  Lighting is done in
	//view space.
	//VMD light motion replaces both when enabled.
	MMDLightState light = currentLight();
	glm::vec3 lightDirView=glm::normalize(glm::mat3(viewMatrix)*light.direction);
	glUniform3f(uniformVars[uLightDirection],lightDirView.x,lightDirView.y,lightDirView.z);
	glUniform3f(uniformVars[uLightColor],light.color.r,light.color.g,light.color.b);
	glUniformMatrix4fv(uniformVars[uMV],1,GL_FALSE,&modelViewMatrix[0][0]);
	glUniform2f(uniformVars[uScreenSize],(float)framebufferWidth,(float)framebufferHeight);
	
	bool selfShadowNow=selfShadowActive();
	int faceCount=0;
	for(int mi=0; mi<pmxInfo->material_continuing_datasets; ++mi) //pmxInfo->material_continuing_datasets
	{
		const VMDMotionController::MaterialState &mat=motionController->materialStates[mi];
		
		//Hidden materials (diffuse alpha 0, typically switched by material
		//morphs) and materials without edges are skipped.
		bool skip = mat.diffuse.a <= 0.0f;
		if(drawEdges && (!pmxInfo->materials[mi]->drawEdges || mat.edgeSize <= 0.0f || mat.edgeColor.a <= 0.0f)) skip = true;
		if(skip)
		{
			faceCount+=pmxInfo->materials[mi]->hasFaceNum;
			continue;
		}
		
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,textureOrWhite(m, pmxInfo->materials[mi]->textureIndex));
		glUniform1i(uniformVars[uTextureSampler], 0);
		
		if((int)pmxInfo->materials[mi]->sphereMode>0)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D,textureOrWhite(m, pmxInfo->materials[mi]->sphereIndex));
			glUniform1i(uniformVars[uSphereSampler], 1);
		}
		
		bool hasToon = pmxInfo->materials[mi]->shareToon==1 ||
		               (pmxInfo->materials[mi]->toonTextureIndex>=0 && pmxInfo->materials[mi]->toonTextureIndex<(int)m.textures.size());
		glUniform1i(uniformVars[uUseToon], hasToon ? 1 : 0);
		
		if((int)pmxInfo->materials[mi]->shareToon==0)
		{
			glActiveTexture(GL_TEXTURE2);
			
			glBindTexture(GL_TEXTURE_2D,textureOrWhite(m, pmxInfo->materials[mi]->toonTextureIndex));
			glUniform1i(uniformVars[uToonSampler], 2);
		}
		else if((int)pmxInfo->materials[mi]->shareToon==1)
		{
			glActiveTexture(GL_TEXTURE2);
			
			glBindTexture(GL_TEXTURE_2D,sharedToon[pmxInfo->materials[mi]->shareToonTexture%10]);
			glUniform1i(uniformVars[uToonSampler], 2);
		}
		
		
		glUniform3fv(uniformVars[uAmbient],1,(GLfloat*)&mat.ambient);
		glUniform4fv(uniformVars[uDiffuse],1,(GLfloat*)&mat.diffuse);
		glUniform3fv(uniformVars[uSpecular],1,(GLfloat*)&mat.specular);
		glUniform4fv(uniformVars[uTextureMul],1,(GLfloat*)&mat.textureMul);
		glUniform4fv(uniformVars[uTextureAdd],1,(GLfloat*)&mat.textureAdd);
		glUniform4fv(uniformVars[uSphereMul],1,(GLfloat*)&mat.sphereMul);
		glUniform4fv(uniformVars[uSphereAdd],1,(GLfloat*)&mat.sphereAdd);
		glUniform4fv(uniformVars[uToonMul],1,(GLfloat*)&mat.toonMul);
		glUniform4fv(uniformVars[uToonAdd],1,(GLfloat*)&mat.toonAdd);
		
		glUniform1f(uniformVars[uShininess],mat.shininess);

		
		glUniform4fv(uniformVars[uEdgeColor],1,(GLfloat*)&mat.edgeColor);
		glUniform1f(uniformVars[uEdgeSize],mat.edgeSize);
		
		//sphere maps only apply when the material actually has a sphere texture
		int sphereIdx=pmxInfo->materials[mi]->sphereIndex;
		bool hasSphere = sphereIdx>=0 && sphereIdx<(int)m.textures.size();
		glUniform1f(uniformVars[uSphereMode], hasSphere ? (float)pmxInfo->materials[mi]->sphereMode : 0.0f);
		glUniform1i(uniformVars[uSelfShadow], (selfShadowNow && !drawEdges && pmxInfo->materials[mi]->drawSelfShadow) ? 1 : 0);
        
		if(drawEdges)
		{
			glEnable(GL_CULL_FACE); //outline = back faces of the inflated mesh
			glCullFace(GL_FRONT);
		}
		else if(pmxInfo->materials[mi]->drawBothSides)
		{
			glDisable(GL_CULL_FACE);
		}
		else
		{
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}
				glDrawElements(GL_TRIANGLES, (pmxInfo->materials[mi]->hasFaceNum), GL_UNSIGNED_INT, BUFFER_OFFSET(sizeof(GLuint)*faceCount));
		faceCount+=pmxInfo->materials[mi]->hasFaceNum;
	}
}


MMDLightState Viewer::currentLight()
{
	return useLightMotion ? lightController.evaluate(currentTime()/2.0f)
	                      : VMDLightController::defaultLight();
}

bool Viewer::selfShadowActive()
{
	if(!useSelfShadow || !shadowFBO) return false;
	if(selfShadowKeys.empty()) return true;
	unsigned frame=(unsigned)(currentTime()/2);
	bool on=selfShadowKeys.front().second;
	for(const auto &k : selfShadowKeys)
	{
		if(k.first>frame) break;
		on=k.second;
	}
	return on;
}

void Viewer::initShadowMap()
{
	shadowFBO=0;
	shadowTexture=0;
	shadowMapSize=2048;
	
	glActiveTexture(GL_TEXTURE0 + SHADOW_TEXTURE_UNIT);
	glGenTextures(1, &shadowTexture);
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowMapSize, shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float border[4]={1.0f, 1.0f, 1.0f, 1.0f}; //outside the map = lit
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glActiveTexture(GL_TEXTURE0);
	
	glGenFramebuffers(1, &shadowFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
	{
		cerr<<"WARNING: shadow map framebuffer not supported; self shadow disabled"<<endl;
		glDeleteFramebuffers(1, &shadowFBO);
		shadowFBO=0;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	//the sampler always has the depth texture bound (also when unused)
	glUniform1i(uniformVars[uShadowMap], SHADOW_TEXTURE_UNIT);
	glUniform2f(uniformVars[uShadowTexelSize], 1.0f/shadowMapSize, 1.0f/shadowMapSize);
	glUniform1i(uniformVars[uRenderMode], 0);
	glUniform1i(uniformVars[uSelfShadow], 0);
	glUniform4f(uniformVars[uGroundShadowColor], 0.0f, 0.0f, 0.0f, 0.4f);
	glm::mat4 identity(1.0f);
	glUniformMatrix4fv(uniformVars[uShadowMVP], 1, GL_FALSE, &identity[0][0]);
	
	//the ground shadow uses the stencil buffer so that overlapping parts
	//do not darken it twice
	GLint stencilBits=0;
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits);
	hasStencil=stencilBits>0;
}

//Orthographic light camera fitted to the current pose (bone positions plus
//a margin for the mesh around them).  The fit is snapped to whole shadow
//map texels so that the shadow edges do not shimmer while the model moves.
glm::mat4 Viewer::lightViewProjection(const glm::vec3 &lightDir)
{
	glm::vec3 bmin(1e30f), bmax(-1e30f);
	float margin=0.0f;
	for(SceneModel *m : models)
	{
		if(!modelShown(*m)) continue;
		glm::mat4 M=modelMatrixFor(*m);
		for(int i=0; i<m->pmxInfo->bone_continuing_datasets; ++i)
		{
			glm::vec3 p=glm::vec3(M*m->pmxInfo->bones[i]->calculateGlobalMatrix()[3]);
			if(!(glm::length(p)<1e6f)) continue; //ignore broken (NaN/far) bones
			bmin=glm::min(bmin, p);
			bmax=glm::max(bmax, p);
		}
		margin=std::max(margin, glm::length(m->restMax-m->restMin)*0.2f);
	}
	if(bmin.x>bmax.x) { bmin=modelMin; bmax=modelMax; }
	glm::vec3 centre=(bmin+bmax)*0.5f;
	float radius=glm::length(bmax-bmin)*0.5f + margin;
	radius=glm::max(1.0f, ceilf(radius*2.0f)*0.5f);
	
	glm::vec3 d=glm::normalize(lightDir);
	glm::vec3 up=(fabsf(d.y)>0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
	glm::mat4 lightView=glm::lookAt(glm::vec3(0.0f), d, up);
	
	glm::vec3 c=glm::vec3(lightView*glm::vec4(centre, 1.0f));
	float texel=2.0f*radius/shadowMapSize;
	c.x=floorf(c.x/texel)*texel;
	c.y=floorf(c.y/texel)*texel;
	//view space looks down -Z: the models are around z=c.z
	glm::mat4 lightProj=glm::ortho(c.x-radius, c.x+radius, c.y-radius, c.y+radius, -c.z-3.0f*radius, -c.z+3.0f*radius);
	return lightProj*lightView;
}

void Viewer::drawShadowCasters(SceneModel &model, int renderMode)
{
	ClosedMMDFormat::PMXInfo *pmxInfo=model.pmxInfo;
	VMDMotionController *motionController=model.motionController;
	glBindVertexArray(model.vao);
	glBindBuffer(GL_ARRAY_BUFFER, model.vertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.indexBuffer);
	motionController->bindBoneTexture();
	glUniform1i(uniformVars[uIsEdge], 0);
	glUniform1i(uniformVars[uRenderMode], renderMode);
	
	int faceCount=0;
	for(int m=0; m<pmxInfo->material_continuing_datasets; ++m)
	{
		const PMXMaterial *pm=pmxInfo->materials[m];
		const VMDMotionController::MaterialState &mat=motionController->materialStates[m];
		bool draw = mat.diffuse.a>0.0f &&
		            (renderMode==1 ? pm->drawToSelfShadowMap : pm->drawGroundShadow);
		if(draw)
		{
			glDrawElements(GL_TRIANGLES, pm->hasFaceNum, GL_UNSIGNED_INT, BUFFER_OFFSET(sizeof(GLuint)*faceCount));
		}
		faceCount+=pm->hasFaceNum;
	}
	glUniform1i(uniformVars[uRenderMode], 0);
}

void Viewer::renderShadowMap(const glm::mat4 &lightVP)
{
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
	glViewport(0, 0, shadowMapSize, shadowMapSize);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.5f, 4.0f);
	
	for(SceneModel *m : models)
	{
		if(!modelShown(*m)) continue;
		glm::mat4 MVP=lightVP*modelMatrixFor(*m);
		glUniformMatrix4fv(MVP_loc, 1, GL_FALSE, &MVP[0][0]);
		drawShadowCasters(*m, 1);
	}
	
	glDisable(GL_POLYGON_OFFSET_FILL);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, framebufferWidth, framebufferHeight);
}

//MMD's ground shadow: the model flattened onto the y=0 plane along the light
void Viewer::drawGroundShadow(const glm::vec3 &lightDir)
{
	glm::vec3 d=glm::normalize(lightDir);
	if(d.y > -0.05f) return; //light from (nearly) below/horizontal: no ground shadow
	
	glm::mat4 flatten(1.0f); //p' = p - d * p.y/d.y
	flatten[1][0]=-d.x/d.y;
	flatten[1][1]=0.0f;
	flatten[1][2]=-d.z/d.y;
	
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if(hasStencil)
	{
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_EQUAL, 0, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
	}
	for(SceneModel *m : models)
	{
		if(!modelShown(*m)) continue;
		glm::mat4 MVP=projectionMatrix*viewMatrix*flatten*modelMatrixFor(*m);
		glUniformMatrix4fv(MVP_loc, 1, GL_FALSE, &MVP[0][0]);
		drawShadowCasters(*m, 2);
	}
	if(hasStencil) glDisable(GL_STENCIL_TEST);
	glDepthMask(GL_TRUE);
}


//#define MODELDUMP
//Fixed attribute indices (must be set before the shader program is linked)
void Viewer::bindAttribLocations()
{
	glBindAttribLocation(shaderProgram, vPosition, "vPosition");
	glBindAttribLocation(shaderProgram, vUV, "vUV");
	glBindAttribLocation(shaderProgram, vNormal, "vNormal");
	glBindAttribLocation(shaderProgram, vBoneIndices, "vBoneIndices");
	glBindAttribLocation(shaderProgram, vBoneWeights, "vBoneWeights");
	glBindAttribLocation(shaderProgram, vWeightFormula, "vWeightFormula");
	glBindAttribLocation(shaderProgram, vSdefC, "vSdefC");
	glBindAttribLocation(shaderProgram, vSdefR0, "vSdefR0");
	glBindAttribLocation(shaderProgram, vSdefR1, "vSdefR1");
}

void Viewer::initBuffers(SceneModel &m)
{
	ClosedMMDFormat::PMXInfo *pmxInfo=m.pmxInfo;
	#ifdef MODELDUMP
	ofstream modeldump("modeldump.txt");
	modeldump << "indices:" << endl;
	#endif
	
	//Note: vertex indices are loaded statically, since they do not change.
	//The actual vertex data is loaded dynamically each frame, so its memory is managed by the MotionController.
	GLuint *vertexIndices= (GLuint*) calloc(pmxInfo->face_continuing_datasets,sizeof(GLuint)*3);
	for(int i=0; i<pmxInfo->faces.size(); ++i) //faces.size()
	{
		int j=i*3;
		vertexIndices[j]=pmxInfo->faces[i]->points[0];
		vertexIndices[j+1]=pmxInfo->faces[i]->points[1];
		vertexIndices[j+2]=pmxInfo->faces[i]->points[2];
		
		#ifdef MODELDUMP
		modeldump << vertexIndices[j] << " " << vertexIndices[j+1] << " " << vertexIndices[j+2] << endl;
		#endif
	}
	
	
	#ifdef MODELDUMP
	modeldump << "vertices:" << endl;
	#endif
	
	#ifdef MODELDUMP
	modeldump.close();
	#endif
	
	//Generate the model's buffers
	glGenBuffers(1, &m.vertexBuffer);
	glGenBuffers(1, &m.indexBuffer);
	
	//init Element Buffer Object
	glGenVertexArrays(1, &m.vao);
	glBindVertexArray(m.vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, pmxInfo->face_continuing_datasets*sizeof(GLuint)*3, vertexIndices, GL_STATIC_DRAW);
	
	free(vertexIndices);
	
	
	//Init Vertex Array Buffer Object
	glBindBuffer(GL_ARRAY_BUFFER, m.vertexBuffer);
	
	//Intialize Vertex Attribute Pointers
	glVertexAttribPointer(vPosition, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(0)); //4=number of components updated per vertex
	glEnableVertexAttribArray(vPosition);

	glVertexAttribPointer(vUV, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(glm::vec4)));
	glEnableVertexAttribArray(vUV);

	glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(glm::vec4)+sizeof(glm::vec2)));
	glEnableVertexAttribArray(vNormal);

	glVertexAttribPointer(vBoneIndices, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(glm::vec4)+sizeof(glm::vec2)+sizeof(glm::vec3)+sizeof(GLfloat)));
	glEnableVertexAttribArray(vBoneIndices);

	glVertexAttribPointer(vBoneWeights, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(glm::vec4)+sizeof(glm::vec2)+sizeof(glm::vec3)+sizeof(GLfloat)*5));
	glEnableVertexAttribArray(vBoneWeights);

	glVertexAttribPointer(vWeightFormula, 1, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(glm::vec4)+sizeof(glm::vec2)+sizeof(glm::vec3)));
	glEnableVertexAttribArray(vWeightFormula);

	glVertexAttribPointer(vSdefC, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(offsetof(VertexData, sdefC)));
	glEnableVertexAttribArray(vSdefC);

	glVertexAttribPointer(vSdefR0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(offsetof(VertexData, sdefR0)));
	glEnableVertexAttribArray(vSdefR0);

	glVertexAttribPointer(vSdefR1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(offsetof(VertexData, sdefR1)));
	glEnableVertexAttribArray(vSdefR1);
}

//Windows-made models often reference textures with different letter case
//than the files on disk.  Resolve each path component case-insensitively.
static string resolvePathCaseInsensitive(const string &path)
{
	if(access(path.c_str(), R_OK)==0) return path;
	
	string result;
	size_t pos=0;
	if(!path.empty() && path[0]=='/') { result="/"; pos=1; }
	
	while(pos<=path.size())
	{
		size_t next=path.find('/', pos);
		if(next==string::npos) next=path.size();
		string component=path.substr(pos, next-pos);
		pos=next+1;
		if(component.empty()) continue;
		
		string dirName=result.empty() ? "." : result;
		string candidate=(result.empty() || result=="/") ? result+component : result+"/"+component;
		if(access(candidate.c_str(), F_OK)!=0)
		{
			DIR *dir=opendir(dirName.c_str());
			if(!dir) return path;
			struct dirent *entry;
			bool found=false;
			while((entry=readdir(dir))!=NULL)
			{
				if(strcasecmp(entry->d_name, component.c_str())==0)
				{
					candidate=(result.empty() || result=="/") ? result+entry->d_name : result+"/"+entry->d_name;
					found=true;
					break;
				}
			}
			closedir(dir);
			if(!found) return path;
		}
		result=candidate;
	}
	return result.empty() ? path : result;
}

//Load one image file into a new GL texture.  Falls back to a 1x1 white
//texture when the file is missing or cannot be decoded.
static GLuint loadTextureFile(const string &requestedPath, bool rgba, bool verbose=true)
{
	string loc=resolvePathCaseInsensitive(requestedPath);
	if(verbose) cout<<"Loading "<<loc<<"...";
	
	GLuint texture=0;
	int width=0, height=0, channels=0;
	//stb_image handles 8/16-bit PNG, JPEG, BMP, TGA, PSD, GIF (.spa/.sph are BMP/PNG files);
	//16-bit images are converted to 8 bits per channel.
	unsigned char *image=stbi_load(loc.c_str(), &width, &height, &channels, rgba ? 4 : 3);
	
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	if(image && width>0 && height>0)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, rgba ? GL_RGBA : GL_RGB, width, height, 0, rgba ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, image);
		if(verbose) cout<<"done"<<endl;
	}
	else
	{
		const unsigned char white[4]={255,255,255,255};
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
		if(verbose) cout<<"failed"<<endl;
		cerr<<"WARNING: texture could not be loaded: "<<requestedPath<<" ("<<(stbi_failure_reason() ? stbi_failure_reason() : "unknown error")<<")"<<endl;
	}
	if(image) stbi_image_free(image);
	
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	//mipmaps: large (e.g. 4096x4096) textures shimmer badly without them
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	
	return texture;
}

static bool hasExtension(const string &path, const char *ext)
{
	size_t n=strlen(ext);
	return path.size()>=n && strcasecmp(path.c_str()+path.size()-n, ext)==0;
}

void Viewer::loadSharedTextures()
{
	//1x1 white texture used for materials without a (valid) texture
	{
		const unsigned char white[4]={255,255,255,255};
		glActiveTexture(GL_TEXTURE0);
		glGenTextures(1, &whiteTexture);
		glBindTexture(GL_TEXTURE_2D, whiteTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	
	//Shared toon textures toon01.bmp - toon10.bmp
	for(int i=1; i<=10; ++i)
	{
		char name[16];
		snprintf(name, sizeof(name), "toon%02d.bmp", i);
		
		string loc=string("data/share/")+name;
		if(access(loc.c_str(), R_OK)!=0)
		{
			loc=string(DATA_PATH)+"/textures/"+name;
		}
		sharedToon[i-1]=loadTextureFile(loc, true, false);
	}
}

void Viewer::loadTextures(SceneModel &m)
{
	for(int i=0; i<m.pmxInfo->texture_continuing_datasets; ++i)
	{
		const string &path=m.pmxInfo->texturePaths[i];
		bool rgba = hasExtension(path,"png") || hasExtension(path,"spa") || hasExtension(path,"tga");
		m.textures.push_back(loadTextureFile(path, rgba));
	}
}


void Viewer::hackShaderFiles()
{
	ifstream test("shaders/model.vert");
	if(!test.is_open())
	{
		shaderProgram=compileShaders(DATA_PATH"/shaders/model.vert",DATA_PATH"/shaders/model.frag");
	}
	else
	{
		shaderProgram=compileShaders("shaders/model.vert","shaders/model.frag");
	}
	test.close();
	
	ifstream test2("shaders/bulletDebug.vert");
	if(!test2.is_open())
	{
		bulletVertPath=DATA_PATH"/shaders/bulletDebug.vert";
		bulletFragPath=DATA_PATH"/shaders/bulletDebug.frag";
	}
	else
	{
		bulletVertPath="shaders/bulletDebug.vert";
		bulletFragPath="shaders/bulletDebug.frag";
	}
}

static void glfwErrorCallback(int error, const char *description)
{
	cerr<<"GLFW error "<<error<<": "<<description<<endl;
}

void Viewer::initGLFW()
{
	glfwSetErrorCallback(glfwErrorCallback);
	if (!glfwInit()) exit(EXIT_FAILURE);
	
	glfwWindowHint(GLFW_SAMPLES, 4); //4x antialiasing
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); //OpenGL 3.2 core (GLSL 1.50) is enough for the shaders
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); //Don't want old OpenGL
	glfwWindowHint(GLFW_DEPTH_BITS, 32);
	glfwWindowHint(GLFW_STENCIL_BITS, 8); //ground shadow (draw each pixel once)
	
	//Open a window and create its OpenGL context
	window = glfwCreateWindow(1280, 720, "PMX Viewer", NULL, NULL);
	if(!window)
	{
		//Some drivers refuse a 32-bit depth buffer; retry with the default
		glfwWindowHint(GLFW_DEPTH_BITS, 24);
		window = glfwCreateWindow(1280, 720, "PMX Viewer", NULL, NULL);
	}
	if(!window)
	{
		fprintf( stderr, "Failed to open GLFW window (OpenGL 3.2 core profile required)\n" );
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

	//GL entry points are resolved by libepoxy on first use; nothing to initialise.
	if(epoxy_gl_version() < 32)
	{
		fprintf(stderr, "OpenGL 3.2 or newer is required (got %d.%d)\n", epoxy_gl_version()/10, epoxy_gl_version()%10);
		exit(EXIT_FAILURE);
	}
	
	//Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	
	glfwSetCursorPosCallback(window, mouseMotion);
	glfwSetScrollCallback(window, mouseWheel);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetDropCallback(window, dropCallback);
	
	GLVersionMajor=glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
	GLVersionMinor=glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
	GLVersionRevision=glfwGetWindowAttrib(window, GLFW_CONTEXT_REVISION);
	GLVersionHintMajor=3; GLVersionHintMinor=2;
	cout<<"OpenGL version info: "<<glGetString(GL_VERSION)<<endl;
	cout<<"GLSL version info: "<<glGetString(GL_SHADING_LANGUAGE_VERSION)<<endl<<endl;
	
	hackShaderFiles();
}

bool Viewer::keyPressed(int key)
{
	if(guiCapturesKeyboard) return false;
	return glfwGetKey(window, key)==GLFW_PRESS;
}

//Key presses are queued by the GLFW key callback so that short taps (and the
//auto-repeat of held keys) are not lost between two polls.
struct KeyEvent { int key, mods; bool repeat; };
static std::vector<KeyEvent> keyEvents;

//Files dropped onto the window are loaded between frames
static void dropCallback(GLFWwindow *window, int count, const char **paths)
{
	(void)window;
	for(int i=0; i<count; ++i) droppedFiles.push_back(paths[i]);
}

static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if(guiCapturesKeyboard) return;
	if(action==GLFW_PRESS || action==GLFW_REPEAT)
	{
		keyEvents.push_back(KeyEvent{key, mods, action==GLFW_REPEAT});
	}
}

GLuint Viewer::textureOrWhite(const SceneModel &m, int index)
{
	if(index<0 || index>=(int)m.textures.size()) return whiteTexture;
	return m.textures[index];
}


void Viewer::initUniformVarLocations()
{
	uniformVars[uAmbient]=glGetUniformLocation(shaderProgram, "ambient");
	uniformVars[uDiffuse]=glGetUniformLocation(shaderProgram, "diffuse");
	uniformVars[uSpecular]=glGetUniformLocation(shaderProgram, "specular");
	
	uniformVars[uShininess]=glGetUniformLocation(shaderProgram, "shininess");
	
	uniformVars[uIsEdge]=glGetUniformLocation(shaderProgram, "isEdge");
	uniformVars[uEdgeColor]=glGetUniformLocation(shaderProgram, "edgeColor");
	uniformVars[uEdgeSize]=glGetUniformLocation(shaderProgram, "edgeSize");
	
	
	uniformVars[uLightDirection]=glGetUniformLocation(shaderProgram, "lightDirection");
	
	uniformVars[uSphereMode]=glGetUniformLocation(shaderProgram, "fSphereMode");
	
	uniformVars[uTextureSampler]=glGetUniformLocation(shaderProgram,"textureSampler");
	uniformVars[uSphereSampler]=glGetUniformLocation(shaderProgram,"sphereSampler");
	uniformVars[uToonSampler]=glGetUniformLocation(shaderProgram,"toonSampler");
	uniformVars[uTextureMul]=glGetUniformLocation(shaderProgram,"textureMul");
	uniformVars[uTextureAdd]=glGetUniformLocation(shaderProgram,"textureAdd");
	uniformVars[uLightColor]=glGetUniformLocation(shaderProgram,"lightColor");
	uniformVars[uUseToon]=glGetUniformLocation(shaderProgram,"useToon");
	uniformVars[uMV]=glGetUniformLocation(shaderProgram,"MV");
	uniformVars[uRenderMode]=glGetUniformLocation(shaderProgram,"renderMode");
	uniformVars[uGroundShadowColor]=glGetUniformLocation(shaderProgram,"groundShadowColor");
	uniformVars[uSelfShadow]=glGetUniformLocation(shaderProgram,"selfShadow");
	uniformVars[uShadowMap]=glGetUniformLocation(shaderProgram,"shadowMap");
	uniformVars[uShadowTexelSize]=glGetUniformLocation(shaderProgram,"shadowTexelSize");
	uniformVars[uShadowMVP]=glGetUniformLocation(shaderProgram,"shadowMVP");
	uniformVars[uScreenSize]=glGetUniformLocation(shaderProgram,"screenSize");
	uniformVars[uSphereMul]=glGetUniformLocation(shaderProgram,"sphereMul");
	uniformVars[uSphereAdd]=glGetUniformLocation(shaderProgram,"sphereAdd");
	uniformVars[uToonMul]=glGetUniformLocation(shaderProgram,"toonMul");
	uniformVars[uToonAdd]=glGetUniformLocation(shaderProgram,"toonAdd");
}

Viewer::~Viewer()
{
	shutdownGUI();
	if(window) glfwDestroyWindow(window);
	glfwTerminate();
}

void Viewer::handleKey(int key, int mods, bool repeat)
{
	bool shift=(mods & GLFW_MOD_SHIFT)!=0;
	switch(key)
	{
		//C toggles the VMD camera motion
		case GLFW_KEY_C:
			if(repeat || cameraController.empty()) break;
			useCameraMotion=!useCameraMotion;
			cout<<(useCameraMotion ? "Camera motion ON" : "Camera motion OFF (free camera)")<<endl;
			break;
		
		//L toggles the VMD light motion
		case GLFW_KEY_L:
			if(repeat || lightController.empty()) break;
			useLightMotion=!useLightMotion;
			cout<<(useLightMotion ? "Light motion ON" : "Light motion OFF (MMD default light)")<<endl;
			break;
		
		//Z: ground shadow on/off, X: self shadow on/off
		case GLFW_KEY_Z:
			if(repeat) break;
			useGroundShadow=!useGroundShadow;
			cout<<"Ground shadow "<<(useGroundShadow ? "ON" : "OFF")<<endl;
			break;
		case GLFW_KEY_X:
			if(repeat) break;
			useSelfShadow=!useSelfShadow;
			cout<<"Self shadow "<<(useSelfShadow ? "ON" : "OFF")<<endl;
			break;
		
		//Space: pause / resume
		case GLFW_KEY_SPACE:
			if(repeat) break;
			setPaused(!paused);
			break;
		
		//Tab: show / hide the on-screen controls
		case GLFW_KEY_TAB:
			if(repeat) break;
			guiVisible=!guiVisible;
			break;
		
		//O: loop on/off
		case GLFW_KEY_O:
			if(repeat) break;
			loopPlayback=!loopPlayback;
			cout<<"Loop "<<(loopPlayback ? "ON" : "OFF")<<endl;
			break;
		
		//Left/Right: seek 1 s (30 frames), with Shift 10 s
		case GLFW_KEY_RIGHT:
			seekTo(currentTime()+(shift ? 600 : 60));
			break;
		case GLFW_KEY_LEFT:
			seekTo(currentTime()-(shift ? 600 : 60));
			break;
		
		//Home/End: beginning / end of the motion
		case GLFW_KEY_HOME:
			seekTo(0);
			break;
		case GLFW_KEY_END:
			seekTo(endTime);
			break;
		
		//Period/Comma: one frame forward/back (pauses playback)
		case GLFW_KEY_PERIOD:
			stepFrame(+1);
			break;
		case GLFW_KEY_COMMA:
			stepFrame(-1);
			break;
	}
}

void Viewer::setPaused(bool p)
{
	if(p==paused) return;
	paused=p;
	soundSetPaused(paused);
	cout<<(paused ? "Paused" : "Playing")<<" at frame "<<currentTime()/2<<endl;
}

void Viewer::stepFrame(int direction)
{
	if(!paused) { paused=true; soundSetPaused(true); }
	if(direction>0)
	{
		stepLogic(); //forward steps keep the physics running continuously
		stepLogic();
		soundSeek(currentTime()/60.0, true);
		updateCameraUniforms();
	}
	else
	{
		seekTo(currentTime()-2);
	}
}

void Viewer::handleEvents()
{
	glfwPollEvents();
	//loads requested in the previous frame (the GUI has shown "loading")
	processPendingLoads();
	for(const std::string &f : droppedFiles) pendingLoads.push_back(PendingLoad{PendingLoad::Auto, f, -1});
	droppedFiles.clear();
	if(!guiAvailable) processPendingLoads(); //no "loading" message to show first
	guiFrame();
	if(keyPressed(GLFW_KEY_R))
	{
		resetCamera();
		useCameraMotion=false;
	}
	
	//one-shot keys (playback control, toggles)
	std::vector<KeyEvent> events;
	events.swap(keyEvents);
	for(const KeyEvent &e : events) handleKey(e.key, e.mods, e.repeat);
	
	//moving the camera with the mouse switches to the free camera
	if(useCameraMotion && (theta!=lastTheta || zenith!=lastZenith || radius!=lastRadius))
	{
		useCameraMotion=false;
		cout<<"Camera motion OFF (free camera; press C to resume)"<<endl;
	}
	lastTheta=theta; lastZenith=zenith; lastRadius=radius;
		if(keyPressed(GLFW_KEY_UP))
	{
		modelTranslate.y-=radius*0.005f;
	}
	else if(keyPressed(GLFW_KEY_DOWN))
	{
		modelTranslate.y+=radius*0.005f;
	}
}


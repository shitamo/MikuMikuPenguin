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
#include "pmxvLogger.h"

#include "motioncontroller.h"
#include "bulletphysics.h"
#include "mmdphysics.h"

#include "sound.h"

#include "glfw_func_callbacks.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//#define MODELDUMP true

using namespace std;
using namespace ClosedMMDFormat;


Viewer::Viewer(string modelPath, string motionPath,string musicPath)
{
	int index=modelPath.rfind("/");
	string modelFilePath,modelFolderPath;
	
	if(index==-1)
	{
		modelFolderPath="";
		modelFilePath=modelPath;
	}
	else
	{
		modelFilePath=modelPath.substr(index);
		modelFolderPath=modelPath.substr(0,index);
	}
	
	pmxInfo=&readPMX(modelFolderPath,modelFilePath);
	vmdInfo=&readVMD(motionPath);
	//pmxInfo=&readPMX("data/model/gumiv3/","GUMI_V3.pmx");
	//vmdInfo=&readVMD("data/motion/Watashi no Jikan/私の時間_short_Lat式ミク.vmd");
	
	//Below is a test for VMD file-writing. Write the current vmdInfo object to a file, then read back the output into a new vmdInfo object
	//writeVMD(*vmdInfo,"testVMD.vmd");
	//vmdInfo=&readVMD("testVMD.vmd");
	
	initGLFW();
	
	
	//NOTE: shaderProgram compilation now handled in hackShaderFiles() within initGLFW()
	/*ifstream test("shaders/model.vert");
	if(!test.is_open())
	{
		shaderProgram=compileShaders(DATA_PATH"/shaders/model.vert",DATA_PATH"/shaders/model.frag");
	}
	else
	{
		shaderProgram=compileShaders("shaders/model.vert","shaders/model.frag");
	}
	test.close();*/
	
	
	
	loadTextures();

	
	initBuffers();
	linkShaders(shaderProgram);
	glUseProgram(shaderProgram);
	

	initUniformVarLocations();
	
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
	//glClearDepth(1.0f);
	
	//glClearColor(0.1f, 0.1f, 0.1f, 0.1f);
	glClearColor(1,1,1,1);
	//glDisable(GL_MULTISAMPLE);
	
	//Setup MotionController, Physics
	glBindBuffer(GL_ARRAY_BUFFER, Buffers[VertexArrayBuffer]);
	motionController=new VMDMotionController(*pmxInfo,*vmdInfo,shaderProgram);
	
	
	initSound(musicPath);
	
	bulletPhysics = new BulletPhysics(bulletVertPath,bulletFragPath);
	glUseProgram(shaderProgram); //restore GL shader program binding to Viewer's shader program after initializing BulletPhysic's debugDrawer
	mmdPhysics = new MMDPhysics(*pmxInfo,motionController,bulletPhysics);
	
	motionController->updateVertexMorphs();
	motionController->updateBoneAnimation();
	
	//Start the physics from the first frame's pose instead of the bind pose
	mmdPhysics->resetToCurrentPose();
	motionController->updateBoneMatrix();
	
	//Initialize timer variables
	startTime = glfwGetTime();
	ticks=0;
	
	modelTranslate=glm::vec3(0.0f,-10.0f,0.0f);
}

void Viewer::handleLogic()
{
	bool doPhysics=true;
	if(keyPressed(GLFW_KEY_Q))
	{
		doPhysics=false;
	}
	
	//if(!keyPressed(GLFW_KEY_A))
	{
		if (!motionController->advanceTime())
		{
			motionController->updateVertexMorphs();
			motionController->updateBoneAnimation();
		}
		//Debug- hold model in bind pose
		//holdModelInBindPose();
		
		mmdPhysics->updateBones(doPhysics);
		motionController->updateBoneMatrix();
		
		glUseProgram(bulletPhysics->debugDrawer->shaderProgram);
		setCamera(bulletPhysics->debugDrawer->MVPLoc);
		glUseProgram(shaderProgram);
		setCamera(MVP_loc);
	}
}

void Viewer::render()
{
	if(!keyPressed(GLFW_KEY_A))
	{
		drawModel(false); //draw model
		drawModel(true); //draw outlines (MMD draws them after the model, depth tested)
		glDisable(GL_CULL_FACE);
	}
	
	if(keyPressed(GLFW_KEY_S))
	{
		bulletPhysics->SetDebugMode(btIDebugDraw::DBG_DrawWireframe);
		bulletPhysics->DebugDrawWorld();
	}
	else if(keyPressed(GLFW_KEY_D))
	{
		bulletPhysics->SetDebugMode(btIDebugDraw::DBG_DrawAabb);
		bulletPhysics->DebugDrawWorld();
	}
	else if(keyPressed(GLFW_KEY_F))
	{
		bulletPhysics->SetDebugMode(btIDebugDraw::DBG_DrawConstraints);
		bulletPhysics->DebugDrawWorld();
	}
	else if(keyPressed(GLFW_KEY_G))
	{
		bulletPhysics->SetDebugMode(btIDebugDraw::DBG_DrawConstraintLimits);
		bulletPhysics->DebugDrawWorld();
	}
	glUseProgram(shaderProgram); //Restore shader program and buffer's to Viewer's after drawing Bullet debug
	glBindVertexArray(VAOs[Vertices]);
	glBindBuffer(GL_ARRAY_BUFFER, Buffers[VertexArrayBuffer]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffers[VertexIndexBuffer]);
	
	//glFinish();
	//glDrawBuffer(RecordBuffer,Buffers);
	//glReadPixels(0,0,1920,1080,GL_RGB,
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
		sprintf(title, "%s | %2d fps", window_name, nb_frames[nbf_now]);
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
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
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
			glUseProgram(bulletPhysics->debugDrawer->shaderProgram);
			setCamera(bulletPhysics->debugDrawer->MVPLoc);
			glUseProgram(shaderProgram);
			setCamera(MVP_loc);
		}
		render();
		
		glfwSwapBuffers(window);
	}
}

void Viewer::setCamera(GLuint MVPLoc)
{
	float aspect = (framebufferWidth>0 && framebufferHeight>0) ? (float)framebufferWidth/(float)framebufferHeight : 16.0f/9.0f;
	glm::mat4 Projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
	// Camera matrix
	glm::mat4 View       = glm::lookAt(
		glm::vec3(cameraPosition.x,cameraPosition.y,-cameraPosition.z), // Camera is at (4,3,3), in World Space
		cameraTarget, // and looks at the origin
		glm::vec3(0,1,0)  // Head is up (set to 0,-1,0 to look upside-down)
	);
	//View= glm::rotate(0.0f,0.0f,0.0f,1.0f)* View;
	// Model matrix : an identity matrix (model will be at the origin)
	glm::mat4 Model = glm::translate(modelTranslate);
	// Our ModelViewProjection : multiplication of our 3 matrices
	glm::mat4 MVP = Projection * View * Model;
	viewMatrix = View;
	modelViewMatrix = View * Model;
	
	glUniformMatrix4fv(MVPLoc, 1, GL_FALSE, &MVP[0][0]);
}

void Viewer::drawModel(bool drawEdges)
{
	//Bind VAO and related Buffers
	glBindVertexArray(VAOs[Vertices]);
	glBindBuffer(GL_ARRAY_BUFFER, Buffers[VertexArrayBuffer]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffers[VertexIndexBuffer]);
	
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
	glm::vec3 lightDirWorld=glm::normalize(glm::vec3(-0.5f,-1.0f,-0.5f));
	glm::vec3 lightDirView=glm::normalize(glm::mat3(viewMatrix)*lightDirWorld);
	glUniform3f(uniformVars[uLightDirection],lightDirView.x,lightDirView.y,lightDirView.z);
	glUniform3f(uniformVars[uLightColor],154.0f/255.0f,154.0f/255.0f,154.0f/255.0f);
	glUniformMatrix4fv(uniformVars[uMV],1,GL_FALSE,&modelViewMatrix[0][0]);
	glUniform2f(uniformVars[uScreenSize],(float)framebufferWidth,(float)framebufferHeight);
	
	int faceCount=0;
	for(int m=0; m<pmxInfo->material_continuing_datasets; ++m) //pmxInfo->material_continuing_datasets
	{
		const VMDMotionController::MaterialState &mat=motionController->materialStates[m];
		
		//Hidden materials (diffuse alpha 0, typically switched by material
		//morphs) and materials without edges are skipped.
		bool skip = mat.diffuse.a <= 0.0f;
		if(drawEdges && (!pmxInfo->materials[m]->drawEdges || mat.edgeSize <= 0.0f || mat.edgeColor.a <= 0.0f)) skip = true;
		if(skip)
		{
			faceCount+=pmxInfo->materials[m]->hasFaceNum;
			continue;
		}
		
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,textureOrWhite(pmxInfo->materials[m]->textureIndex));
		glUniform1i(uniformVars[uTextureSampler], 0);
		
		if((int)pmxInfo->materials[m]->sphereMode>0)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D,textureOrWhite(pmxInfo->materials[m]->sphereIndex));
			glUniform1i(uniformVars[uSphereSampler], 1);
		}
		
		bool hasToon = pmxInfo->materials[m]->shareToon==1 ||
		               (pmxInfo->materials[m]->toonTextureIndex>=0 && pmxInfo->materials[m]->toonTextureIndex<(int)textures.size()-10);
		glUniform1i(uniformVars[uUseToon], hasToon ? 1 : 0);
		
		if((int)pmxInfo->materials[m]->shareToon==0)
		{
			glActiveTexture(GL_TEXTURE2);
			
			glBindTexture(GL_TEXTURE_2D,textureOrWhite(pmxInfo->materials[m]->toonTextureIndex));
			glUniform1i(uniformVars[uToonSampler], 2);
		}
		else if((int)pmxInfo->materials[m]->shareToon==1)
		{
			glActiveTexture(GL_TEXTURE2);
			
			glBindTexture(GL_TEXTURE_2D,textureOrWhite((int)textures.size()-10+(int)pmxInfo->materials[m]->shareToonTexture));
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
		int sphereIdx=pmxInfo->materials[m]->sphereIndex;
		bool hasSphere = sphereIdx>=0 && sphereIdx<(int)textures.size()-10;
		glUniform1f(uniformVars[uSphereMode], hasSphere ? (float)pmxInfo->materials[m]->sphereMode : 0.0f);
        
		if(drawEdges)
		{
			glEnable(GL_CULL_FACE); //outline = back faces of the inflated mesh
			glCullFace(GL_FRONT);
		}
		else if(pmxInfo->materials[m]->drawBothSides)
		{
			glDisable(GL_CULL_FACE);
		}
		else
		{
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}
				glDrawElements(GL_TRIANGLES, (pmxInfo->materials[m]->hasFaceNum), GL_UNSIGNED_INT, BUFFER_OFFSET(sizeof(GLuint)*faceCount));
		faceCount+=pmxInfo->materials[m]->hasFaceNum;
	}
}


//#define MODELDUMP
void Viewer::initBuffers()
{
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
	
	//Generate all Viewer Buffers
	glGenBuffers(NumBuffers,Buffers);
	
	//init Element Buffer Object
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffers[VertexIndexBuffer]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, pmxInfo->face_continuing_datasets*sizeof(GLuint)*3, vertexIndices, GL_STATIC_DRAW);
	
	free(vertexIndices);
	
	
	//Init Vertex Array Buffer Object
	glGenVertexArrays(NumVAOs, VAOs);
	glBindVertexArray(VAOs[Vertices]);
	
	glBindBuffer(GL_ARRAY_BUFFER, Buffers[VertexArrayBuffer]);
	
	//Intialize Vertex Attribute Pointers
	glVertexAttribPointer(vPosition, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(0)); //4=number of components updated per vertex
	glBindAttribLocation(shaderProgram, vPosition, "vPosition"); //Explicit vertex attribute index specification for older OpenGL version support. (Newer method is layout qualifier in vertex shader)
	glEnableVertexAttribArray(vPosition);

	glVertexAttribPointer(vUV, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(glm::vec4)));
	glBindAttribLocation(shaderProgram, vUV, "vUV");
	glEnableVertexAttribArray(vUV);

	glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(glm::vec4)+sizeof(glm::vec2)));
	glBindAttribLocation(shaderProgram, vNormal, "vNormal");
	glEnableVertexAttribArray(vNormal);

	glVertexAttribPointer(vBoneIndices, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(glm::vec4)+sizeof(glm::vec2)+sizeof(glm::vec3)+sizeof(GLfloat)));
	glBindAttribLocation(shaderProgram, vBoneIndices, "vBoneIndices");
	glEnableVertexAttribArray(vBoneIndices);

	glVertexAttribPointer(vBoneWeights, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(glm::vec4)+sizeof(glm::vec2)+sizeof(glm::vec3)+sizeof(GLfloat)*5));
	glBindAttribLocation(shaderProgram, vBoneWeights, "vBoneWeights");
	glEnableVertexAttribArray(vBoneWeights);

	glVertexAttribPointer(vWeightFormula, 1, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(glm::vec4)+sizeof(glm::vec2)+sizeof(glm::vec3)));
	glBindAttribLocation(shaderProgram, vWeightFormula, "vWeightFormula");
	glEnableVertexAttribArray(vWeightFormula);

	glVertexAttribPointer(vSdefC, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(offsetof(VertexData, sdefC)));
	glBindAttribLocation(shaderProgram, vSdefC, "vSdefC");
	glEnableVertexAttribArray(vSdefC);

	glVertexAttribPointer(vSdefR0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(offsetof(VertexData, sdefR0)));
	glBindAttribLocation(shaderProgram, vSdefR0, "vSdefR0");
	glEnableVertexAttribArray(vSdefR0);

	glVertexAttribPointer(vSdefR1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(offsetof(VertexData, sdefR1)));
	glBindAttribLocation(shaderProgram, vSdefR1, "vSdefR1");
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

void Viewer::loadTextures()
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
	
	for(int i=0; i<pmxInfo->texture_continuing_datasets; ++i)
	{
		const string &path=pmxInfo->texturePaths[i];
		bool rgba = hasExtension(path,"png") || hasExtension(path,"spa") || hasExtension(path,"tga");
		textures.push_back(loadTextureFile(path, rgba));
	}
	
	//Shared toon textures toon01.bmp - toon10.bmp (always the last 10 entries)
	for(int i=1; i<=10; ++i)
	{
		char name[16];
		snprintf(name, sizeof(name), "toon%02d.bmp", i);
		
		string loc=string("data/share/")+name;
		if(access(loc.c_str(), R_OK)!=0)
		{
			loc=string(DATA_PATH)+"/textures/"+name;
		}
		textures.push_back(loadTextureFile(loc, true, false));
	}
}


//Load the shader files (a "shaders/" directory in the current directory
//takes precedence over the installed copies).
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
	return glfwGetKey(window, key)==GLFW_PRESS;
}

GLuint Viewer::textureOrWhite(int index)
{
	if(index<0 || index>=(int)textures.size()) return whiteTexture;
	return textures[index];
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
	uniformVars[uScreenSize]=glGetUniformLocation(shaderProgram,"screenSize");
	uniformVars[uSphereMul]=glGetUniformLocation(shaderProgram,"sphereMul");
	uniformVars[uSphereAdd]=glGetUniformLocation(shaderProgram,"sphereAdd");
	uniformVars[uToonMul]=glGetUniformLocation(shaderProgram,"toonMul");
	uniformVars[uToonAdd]=glGetUniformLocation(shaderProgram,"toonAdd");
}

Viewer::~Viewer()
{
	if(window) glfwDestroyWindow(window);
	glfwTerminate();
}

void Viewer::handleEvents()
{
	glfwPollEvents();
	if(keyPressed(GLFW_KEY_UP))
	{
		modelTranslate.y-=0.1;
	}
	else if(keyPressed(GLFW_KEY_DOWN))
	{
		modelTranslate.y+=0.1;
	}
}

void Viewer::holdModelInBindPose()
{
	for(unsigned i = 0; i<pmxInfo->bone_continuing_datasets; i++)
	{
		PMXBone *b=pmxInfo->bones[i];
		
		if(b->parentBoneIndex!=-1)
		{
			PMXBone *parent = pmxInfo->bones[b->parentBoneIndex];
			b->Local = glm::translate( b->position - parent->position );
		}
		else
		{
			b->Local = glm::translate( b->position );
		}
	
		motionController->skinMatrix[i] = b->calculateGlobalMatrix()*motionController->invBindPose[i];
	}
	motionController->updateBoneMatrix();
}



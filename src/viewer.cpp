#include "viewer.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <SOIL/SOIL.h>

#include <cstdio>
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
	
	initGLFW();
	
	ifstream test("shaders/model.vert");
	if(!test.is_open())
	{
		shaderProgram=loadShaders(DATA_PATH"/shaders/model.vert",DATA_PATH"/shaders/model.frag");
	}
	else
	{
		shaderProgram=loadShaders("shaders/model.vert","shaders/model.frag");
	}
	test.close();
	
	
	
	glUseProgram(shaderProgram);
	loadTextures();
	initBuffers();
	initUniformVarLocations();
	
	MVP_loc = glGetUniformLocation(shaderProgram, "MVP");
    
	//Set OpenGL render settings
    glDisable(GL_CULL_FACE);
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
	
	
	ifstream test2("shaders/bulletDebug.vert");
	GLuint btDebugShaderProgram;
	if(!test2.is_open())
	{
		btDebugShaderProgram=loadShaders(DATA_PATH"/shaders/bulletDebug.vert",DATA_PATH"/shaders/bulletDebug.frag");
	}
	else
	{
		btDebugShaderProgram=loadShaders("shaders/bulletDebug.vert","shaders/bulletDebug.frag");
	}
	//GLuint btDebugShaderProgram=loadShaders(DATA_PATH"/shaders/bulletDebug.vert",DATA_PATH"/shaders/bulletDebug.frag");
	
	initSound(musicPath);
	
	bulletPhysics = new BulletPhysics(btDebugShaderProgram);
	glUseProgram(shaderProgram); //restore GL shader program to Viewer's shader program after initializing BulletPhysic's debugDrawer
	mmdPhysics = new MMDPhysics(*pmxInfo,motionController,bulletPhysics);
	
	motionController->updateVertexMorphs();
	motionController->updateBoneAnimation();
	
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
		motionController->advanceTime();
		
		//Debug- hold model in bind pose
		//holdModelInBindPose();
		
		motionController->updateVertexMorphs();
		motionController->updateBoneAnimation();
		mmdPhysics->updateBones(doPhysics);
		
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
		drawModel(true); //draw model edges
		drawModel(false); //draw model
	}
	
	if(keyPressed(GLFW_KEY_S))
	{
		bulletPhysics->setDebugMode(btIDebugDraw::DBG_DrawWireframe);
		bulletPhysics->DebugDrawWorld();
	}
	else if(keyPressed(GLFW_KEY_D))
	{
		bulletPhysics->setDebugMode(btIDebugDraw::DBG_DrawAabb);
		bulletPhysics->DebugDrawWorld();
	}
	else if(keyPressed(GLFW_KEY_F))
	{
		bulletPhysics->setDebugMode(btIDebugDraw::DBG_DrawConstraints);
		bulletPhysics->DebugDrawWorld();
	}
	else if(keyPressed(GLFW_KEY_G))
	{
		bulletPhysics->setDebugMode(btIDebugDraw::DBG_DrawConstraintLimits);
		bulletPhysics->DebugDrawWorld();
	}
	glUseProgram(shaderProgram); //Restore shader program and buffer's to Viewer's after drawing Bullet debug
	glBindVertexArray(VAOs[Vertices]);
	glBindBuffer(GL_ARRAY_BUFFER, Buffers[VertexArrayBuffer]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffers[VertexIndexBuffer]);
	
	//drawIKMarkers();

	//glFinish();
	//glDrawBuffer(RecordBuffer,Buffers);
	//glReadPixels(0,0,1920,1080,GL_RGB,
}

void Viewer::run()
{
	while(!keyPressed(GLFW_KEY_ESCAPE) && !glfwWindowShouldClose(window))
	{
		glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
		glViewport(0, 0, framebufferWidth, framebufferHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		startTime = glfwGetTime();
		
		handleEvents();
		handleLogic();
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
        
        glDisable(GL_DEPTH_TEST);
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
	
	
	glm::vec3 halfVector=glm::normalize(cameraPosition-cameraTarget);
	halfVector.z=-halfVector.z;
	GLuint halfVectorLoc=glGetUniformLocation(shaderProgram, "halfVector");
	
	glm::vec3 lightDirection=glm::normalize(glm::vec3(0.3,1.0,2.0));
	GLuint lightDirectionLoc=glGetUniformLocation(shaderProgram, "lightDirection");
	
	int faceCount=0;
	for(int m=0; m<pmxInfo->material_continuing_datasets; ++m) //pmxInfo->material_continuing_datasets
	{
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,textureOrWhite(pmxInfo->materials[m]->textureIndex));
		glUniform1i(uniformVars[uTextureSampler], 0);
		
		if((int)pmxInfo->materials[m]->sphereMode>0)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D,textureOrWhite(pmxInfo->materials[m]->sphereIndex));
			glUniform1i(uniformVars[uSphereSampler], 1);
		}
		
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
		
		
		glUniform3fv(uniformVars[uAmbient],1,(GLfloat*)&pmxInfo->materials[m]->ambient);
		glUniform4fv(uniformVars[uDiffuse],1,(GLfloat*)&pmxInfo->materials[m]->diffuse);
		glUniform3fv(uniformVars[uSpecular],1,(GLfloat*)&pmxInfo->materials[m]->specular);
		
		glUniform1f(uniformVars[uShininess],mmp::scalarNormalize(pmxInfo->materials[m]->shininess));
		glUniform3f(uniformVars[uHalfVector],halfVector.x,halfVector.y,halfVector.z);
		glUniform3f(uniformVars[uLightDirection],lightDirection.x,lightDirection.y,lightDirection.z);
		
		glUniform4fv(uniformVars[uEdgeColor],1,(GLfloat*)&pmxInfo->materials[m]->edgeColor);
		glUniform1f(uniformVars[uEdgeSize],mmp::scalarNormalize(pmxInfo->materials[m]->edgeSize));
		
		glUniform1f(uniformVars[uSphereMode],mmp::scalarNormalize(pmxInfo->materials[m]->sphereMode));
        
		glDrawElements(GL_TRIANGLES, (pmxInfo->materials[m]->hasFaceNum), GL_UNSIGNED_INT, BUFFER_OFFSET(sizeof(GLuint)*faceCount));
		faceCount+=pmxInfo->materials[m]->hasFaceNum;
	}
}

void Viewer::drawIKMarkers()
{
	glBindVertexArray(VAOs[IKDebugVertices]);
	glBindBuffer(GL_ARRAY_BUFFER, Buffers[IKVertexArrayBuffer]);
	
	
	vector<int> targetBoneIndices;
	
	for(int i=0; i<pmxInfo->bone_continuing_datasets; ++i)
	{
		PMXBone *b=pmxInfo->bones[i];
	
		glm::mat4 tmpMatrix=b->calculateGlobalMatrix();
		IKVertexData[i].position.x=tmpMatrix[3][0];
		IKVertexData[i].position.y=tmpMatrix[3][1];
		IKVertexData[i].position.z=tmpMatrix[3][2];
		IKVertexData[i].position.w=tmpMatrix[3][3];
		
		//IKVertexData[i].position.x=b->Global[3][0];
		//IKVertexData[i].position.y=b->Global[3][1];
		//IKVertexData[i].position.z=b->Global[3][2];
		//IKVertexData[i].position.w=b->Global[3][3];

		IKVertexData[i].UV.x=0;
		IKVertexData[i].UV.y=0;
		
		if(b->IK)
		{
			targetBoneIndices.push_back(b->IKTargetBoneIndex);
			
			IKVertexData[i].normal.x=1;
			IKVertexData[i].normal.y=0;
			IKVertexData[i].normal.z=0;
		}
		else
		{
			IKVertexData[i].normal.x=0;
			IKVertexData[i].normal.y=0;
			IKVertexData[i].normal.z=0;
		}

		IKVertexData[i].weightFormula=4;

		IKVertexData[i].boneIndex1=0;
		IKVertexData[i].boneIndex2=0;
		IKVertexData[i].boneIndex3=0;
		IKVertexData[i].boneIndex4=0;

		IKVertexData[i].weight1=0;
		IKVertexData[i].weight2=0;
		IKVertexData[i].weight3=0;
		IKVertexData[i].weight4=0;
		
	}
	
	for(int i=0; i<targetBoneIndices.size(); ++i)
	{
		IKVertexData[ targetBoneIndices[i] ].normal.x=0;
		IKVertexData[ targetBoneIndices[i] ].normal.y=1;
		IKVertexData[ targetBoneIndices[i] ].normal.z=0;
	}
	
	glBufferData(GL_ARRAY_BUFFER, pmxInfo->bone_continuing_datasets*sizeof(VertexData), IKVertexData, GL_DYNAMIC_DRAW);
	
	GLuint Bones_loc=glGetUniformLocation(shaderProgram,"Bones");
	glm::mat4 tmpSkinMatrix[pmxInfo->bone_continuing_datasets];
	
	glUniformMatrix4fv(Bones_loc, pmxInfo->bone_continuing_datasets, GL_FALSE, (const GLfloat*)&tmpSkinMatrix);
	
	glDrawArrays(GL_POINTS, 0, pmxInfo->bone_continuing_datasets);
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
	glEnableVertexAttribArray(vPosition);
	
	glVertexAttribPointer(vUV, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(GLfloat)*4));
	glEnableVertexAttribArray(vUV);
	
	glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(GLfloat)*6));
	glEnableVertexAttribArray(vNormal);
	
	glVertexAttribPointer(vBoneIndices, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(GLfloat)*10));
	glEnableVertexAttribArray(vBoneIndices);
	
	glVertexAttribPointer(vBoneWeights, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(GLfloat)*14));
	glEnableVertexAttribArray(vBoneWeights);
	
	glVertexAttribPointer(vWeightFormula, 1, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(GLfloat)*9));
	glEnableVertexAttribArray(vWeightFormula);	
	
	
	//Initialize IK Debug buffers
	IKVertexData = (VertexData*)malloc(pmxInfo->bone_continuing_datasets*sizeof(VertexData));

	glBindVertexArray(VAOs[IKDebugVertices]);
	
	glBindBuffer(GL_ARRAY_BUFFER, Buffers[IKVertexArrayBuffer]);
	
	glVertexAttribPointer(vPosition, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(0)); //4=number of components updated per vertex
	glEnableVertexAttribArray(vPosition);
	
	glVertexAttribPointer(vUV, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(GLfloat)*4));
	glEnableVertexAttribArray(vUV);
	
	glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(GLfloat)*6));
	glEnableVertexAttribArray(vNormal);
	
	glVertexAttribPointer(vBoneIndices, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(GLfloat)*10));
	glEnableVertexAttribArray(vBoneIndices);
	
	glVertexAttribPointer(vBoneWeights, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(GLfloat)*14));
	glEnableVertexAttribArray(vBoneWeights);
	
	glVertexAttribPointer(vWeightFormula, 1, GL_FLOAT, GL_FALSE, sizeof(VertexData), BUFFER_OFFSET(sizeof(GLfloat)*9));
	glEnableVertexAttribArray(vWeightFormula);
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
	unsigned char *image=SOIL_load_image(loc.c_str(), &width, &height, &channels, rgba ? SOIL_LOAD_RGBA : SOIL_LOAD_RGB);
	
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
		cerr<<"WARNING: texture could not be loaded: "<<requestedPath<<" ("<<SOIL_last_result()<<")"<<endl;
	}
	if(image) SOIL_free_image_data(image);
	
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
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


static void glfwErrorCallback(int error, const char *description)
{
	cerr<<"GLFW error "<<error<<": "<<description<<endl;
}

void Viewer::initGLFW()
{
	glfwSetErrorCallback(glfwErrorCallback);
	if (!glfwInit()) exit(EXIT_FAILURE);
	
	glfwWindowHint(GLFW_SAMPLES, 4); //4x antialiasing
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); //OpenGL 3.3 core is enough for the shaders
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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
		fprintf( stderr, "Failed to open GLFW window (OpenGL 3.3 core profile required)\n" );
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

	// Initialize GLEW
	glewExperimental=GL_TRUE; //Needed in core profile
	GLenum glewStatus=glewInit();
#ifdef GLEW_ERROR_NO_GLX_DISPLAY
	//GLEW built for GLX reports this under Wayland/EGL even though the
	//function pointers were loaded successfully.
	if(glewStatus==GLEW_ERROR_NO_GLX_DISPLAY) glewStatus=GLEW_OK;
#endif
	if(glewStatus != GLEW_OK)
	{
		fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(glewStatus));
		exit(EXIT_FAILURE);
	}
	while(glGetError()!=GL_NO_ERROR) {} //glewInit may leave a harmless GL_INVALID_ENUM
	
	//Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	
	glfwSetCursorPosCallback(window, mouseMotion);
	glfwSetScrollCallback(window, mouseWheel);
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
	
	uniformVars[uHalfVector]=glGetUniformLocation(shaderProgram, "halfVector");
	
	uniformVars[uLightDirection]=glGetUniformLocation(shaderProgram, "lightDirection");
	
	uniformVars[uSphereMode]=glGetUniformLocation(shaderProgram, "fSphereMode");
	
	uniformVars[uTextureSampler]=glGetUniformLocation(shaderProgram,"textureSampler");
	uniformVars[uSphereSampler]=glGetUniformLocation(shaderProgram,"sphereSampler");
	uniformVars[uToonSampler]=glGetUniformLocation(shaderProgram,"toonSampler");
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



#ifndef MOTION_CONTROLLER_H
#define MOTION_CONTROLLER_H

#include <GL/glew.h>
#define GLFW_INCLUDE_GLU
#include <GL/glfw.h>

#include "pmx.h"
#include "vmd.h"

#include <list>
#include <sstream>

extern glm::quat IKQuat;

struct VertexData
{
	glm::vec4 position;
	glm::vec2 UV;
	glm::vec3 normal;

	GLfloat weightFormula;

	GLfloat boneIndex1;
	GLfloat boneIndex2;
	GLfloat boneIndex3;
	GLfloat boneIndex4;

	GLfloat weight1;
	GLfloat weight2;
	GLfloat weight3;
	GLfloat weight4;

	std::string str()
	{	
		std::stringstream ss;
		ss << position.x << " " << position.y << " " << position.z << " " << position.w << std::endl;
		ss << UV.x << " " << UV.y << std::endl;
		ss << normal.x << " " << normal.y << " " << normal.z << std::endl;
		ss << boneIndex1 << " " << boneIndex2 << " " << boneIndex3 << " " << boneIndex4 << std::endl;
		ss << weight1 << " " << weight2 << " " << weight3 << " " << weight4 << std::endl;

		return ss.str();
	}
};

class VMDMotionController
{
	public:
	VMDMotionController(PMXInfo &pmxInfovar,VMDInfo &vmdInfovar,GLuint shaderProgram);
	~VMDMotionController();
	void updateVertexMorphs();
	void updateBoneMatrix();
	void updateBoneAnimation();
	void advanceTime();
	
	
	private:
	int time;
	
	void updateChildren(PMXBone *linkBone);
	void updateIK();
	void solveIK(PMXBone *joint, int effector, glm::vec3 targetPos, int numIteration, int numBones, int numMaxBones);
	
	
	PMXInfo &pmxInfo;
	VMDInfo &vmdInfo;
	
	//***BONE TRANSFORMATION VARIABLES***
	public:
	glm::mat4 *skinMatrix; //Final Skinning Matrix
	glm::mat4 *invBindPose;
	
	private:
	GLuint Bones_loc; //uniform variable location for Bones[] in shader
	
	std::vector<std::list<BoneFrame>> boneKeyFrames;				//Hold a list of keyframes for each Bone
	std::vector<std::list<BoneFrame>::iterator> ite_boneKeyFrames;	//Keyframe iterator
	
	std::vector<glm::quat> boneRot;
	std::vector<glm::vec3> bonePos;	
	
	//***VERTEX MORPH VARIABLES
	public: VertexData *vertexData;
	private:
	std::vector<std::list<MorphFrame>> morphKeyFrames;				//Hold a list of keyframes for each vertex morph
	std::vector<std::list<MorphFrame>::iterator> ite_morphKeyFrames;	//Keyframe iterator
	
	std::vector<float> vMorphWeights;
};


#endif

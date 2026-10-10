#ifndef MMD_PHYSICS_H
#define MMD_PHYSICS_H

#include "bulletphysics.h"
#include "pmx.h"
#include "vmd.h"
#include "motioncontroller.h"

#include <vector>
#include <string>
#include <fstream>

#include "mmp_glm.h"

class MMDPhysics
{
	public:
	MMDPhysics(PMXInfo &pmxInfo, VMDMotionController *motionController, BulletPhysics* bulletPhysics);
	
	glm::mat4 createRigidMatrix(glm::vec3 &pos, glm::vec3 &rot, int &i);
	
	void createRigidBody();
	void createJoints();
	void updateBones(bool physicsEnabled);
	
	
	std::vector<int> rigidMeshIndices;
	std::vector<std::vector<glm::vec3> > rigidMeshes;
	
	std::vector<std::vector<glm::vec3> > jointMeshes;
	
	std::vector<btRigidBody*> rigidBodies;
	
	private:
	BulletPhysics* bulletPhysics;
	VMDMotionController *motionController;
	PMXInfo &pmxInfo;
};


#endif

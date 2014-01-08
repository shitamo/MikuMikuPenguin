#include "mmdphysics.h"

#include <iostream>

using namespace std;

MMDPhysics::MMDPhysics(PMXInfo &pmxInfo, VMDMotionController *motionController, BulletPhysics* bulletPhysics):pmxInfo(pmxInfo),motionController(motionController)
{
	this->bulletPhysics=bulletPhysics;
	//this->joint_mesh = 0;
	
	createRigidBody();
	createJoints();
	
	//createTestRigidBody
	/*glm::mat4 world;
	int i=-1;
	glm::vec3 position(0,5.0,0);
	glm::vec3 rotation(0,0.0,0);
	
	world = createRigidMatrix(position, rotation, i);
	
	float radius=5.f;
	float mass=5.f;
	float elasticity=5.f;
	float friction=5.f;
	float movementDecay=5.f;
	float rotationDecay=5.f;
	bool physicsOperation=true;
	int group=0;
	bool noCollisionGroupFlag=false;
			
	this->rigidBodies.push_back(bulletPhysics->CreateSphere(
radius, &world, mass, elasticity, friction, movementDecay,
rotationDecay, physicsOperation == 0, 1 << group, noCollisionGroupFlag));*/

	//createTestJoint
}

glm::mat4 MMDPhysics::createRigidMatrix(glm::vec3 &pos, glm::vec3 &rot, int &i)
{
	glm::vec3 p = glm::vec3(pos);
	if(i!=-1)
	{
		//p += pmxInfo.bones[i]->parent->position; //p += glm::vec3((*bones)[i].initMatML.m[3]); //関連ボーンがある場合は、ボーン相対座標からモデルローカル座標に変換。MmdStruct::PmdRigidBody.pos_posを参照
		//(*bones)[i].extraBoneControl = true;
	}
	glm::mat4 trans, rotation;
	trans=glm::translate(p); //D3DXMatrixTranslation(&trans, p.x, p.y, p.z);
	glm::vec3 r=rot;
	rotation=glm::yawPitchRoll(r.y,r.x,r.z); //D3DXMatrixRotationYawPitchRoll(&rotation, r.y, r.x, r.z);
	return trans*rotation;
}

vector<glm::vec3> createBox(float width, float height, float depth)
{
	vector<glm::vec3> mesh;
	
	mesh.push_back(glm::vec3(-width,height,-depth));
	mesh.push_back(glm::vec3(-width,-height,-depth));
	mesh.push_back(glm::vec3(width,-height,-depth));
	
	mesh.push_back(glm::vec3(width,-height,-depth));
	mesh.push_back(glm::vec3(width,height,-depth));
	mesh.push_back(glm::vec3(-width,height,-depth));
	
	mesh.push_back(glm::vec3(-width,height,depth));
	mesh.push_back(glm::vec3(-width,-height,depth));
	mesh.push_back(glm::vec3(width,-height,depth));
	
	mesh.push_back(glm::vec3(width,-height,depth));
	mesh.push_back(glm::vec3(width,height,depth));
	mesh.push_back(glm::vec3(-width,height,depth));
	
	
	mesh.push_back(glm::vec3(width,height,-depth));
	mesh.push_back(glm::vec3(width,height,depth));
	mesh.push_back(glm::vec3(width,-height,depth));
	
	mesh.push_back(glm::vec3(width,-height,depth));
	mesh.push_back(glm::vec3(width,-height,-depth));
	mesh.push_back(glm::vec3(width,height,-depth));
	
	mesh.push_back(glm::vec3(-width,height,-depth));
	mesh.push_back(glm::vec3(-width,height,depth));
	mesh.push_back(glm::vec3(-width,-height,depth));
	
	mesh.push_back(glm::vec3(-width,-height,depth));
	mesh.push_back(glm::vec3(-width,-height,-depth));
	mesh.push_back(glm::vec3(-width,height,-depth));
	
	
	mesh.push_back(glm::vec3(width,height,-depth));
	mesh.push_back(glm::vec3(width,height,depth));
	mesh.push_back(glm::vec3(-width,height,depth));
	
	mesh.push_back(glm::vec3(-width,height,depth));
	mesh.push_back(glm::vec3(-width,height,-depth));
	mesh.push_back(glm::vec3(width,height,-depth));
	
	mesh.push_back(glm::vec3(width,-height,-depth));
	mesh.push_back(glm::vec3(width,-height,depth));
	mesh.push_back(glm::vec3(-width,-height,depth));
	
	mesh.push_back(glm::vec3(-width,-height,depth));
	mesh.push_back(glm::vec3(-width,-height,-depth));
	mesh.push_back(glm::vec3(width,-height,-depth));
	
	/*mesh.push_back(glm::vec3(-width,height,depth));
	mesh.push_back(glm::vec3(-width,-height,depth));
	mesh.push_back(glm::vec3(width,height,depth));
	mesh.push_back(glm::vec3(width,-height,depth));*/
	
	return mesh;
}

void MMDPhysics::createRigidBody()
{
	for(int i=0; i<pmxInfo.rigid_body_continuing_datasets; ++i)
	{		
		std::vector<PMXRigidBody*> &rigidBodies=pmxInfo.rigidBodies;
		
		glm::mat4 world, world_inv;
		world = createRigidMatrix(rigidBodies[i]->position, rigidBodies[i]->rotation, rigidBodies[i]->relatedBoneIndex);
		world_inv = glm::inverse(world);
		rigidBodies[i]->Init=world;
		rigidBodies[i]->Offset=world_inv;
		
		if (rigidBodies[i]->shape == RIGID_SHAPE_SPHERE) //球
		{
			float radius = rigidBodies[i]->size.x/2;
			
			this->rigidBodies.push_back(bulletPhysics->CreateSphere(
radius, &world, rigidBodies[i]->mass, rigidBodies[i]->elasticity, rigidBodies[i]->friction, rigidBodies[i]->movementDecay,
rigidBodies[i]->rotationDecay, rigidBodies[i]->physicsOperation == 0, 1 << rigidBodies[i]->group, rigidBodies[i]->noCollisionGroupFlag));
				
			const unsigned int slices = 10, stacks = 5;
			//createSphere(radius,slices,stacks); //D3DXCreateSphere(pDevice, radius, slices, stacks, &mesh, 0);
			//rigidbody_mesh.push_back(mesh);
		}
		else if (rigidBodies[i]->shape == RIGID_SHAPE_CUBE) //箱
		{
			float width = rigidBodies[i]->size.x, height = rigidBodies[i]->size.y, depth = rigidBodies[i]->size.z;
			
			this->rigidBodies.push_back(bulletPhysics->CreateBox(
width, height, depth, &world, rigidBodies[i]->mass, rigidBodies[i]->elasticity, rigidBodies[i]->friction, rigidBodies[i]->movementDecay,
rigidBodies[i]->rotationDecay, rigidBodies[i]->physicsOperation == 0, 1 << rigidBodies[i]->group, rigidBodies[i]->noCollisionGroupFlag));

			//D3DXCreateBox(pDevice, width, height, depth, &mesh, 0);
			//vector<glm::vec3> mesh=createBox(width,height,depth);
			//rigidMeshes.push_back(mesh);
			//rigidMeshIndices.push_back(i);
		}
		else if(rigidBodies[i]->shape == RIGID_SHAPE_CAPSULE) //カプセル
		{
			float radius = rigidBodies[i]->size.x/2, height = rigidBodies[i]->size.y/2;
			
			this->rigidBodies.push_back(bulletPhysics->CreateCapsule(
radius, height, &world, rigidBodies[i]->mass, rigidBodies[i]->elasticity, rigidBodies[i]->friction, rigidBodies[i]->movementDecay,
rigidBodies[i]->rotationDecay, rigidBodies[i]->physicsOperation == 0, 1 << rigidBodies[i]->group, rigidBodies[i]->noCollisionGroupFlag));
				
			const unsigned int slices = 10, stacks = 5;
			//D3DXCreateCylinder(pDevice, radius, radius, height, slices, stacks, &mesh, 0);	// カプセル形状の代わりに円柱で表示
			//createCylinder(radius,height,slices,stacks);
			//rigidbody_mesh.push_back(mesh);
		}
	}
}

void MMDPhysics::createJoints()
{
	for (unsigned int i = 0; i<pmxInfo.joint_continuing_datasets; ++i)
	{
		PMXJoint *joint=pmxInfo.joints[i];
		
		glm::vec3 c_p1=(joint->movementLowerLimit);
		glm::vec3 c_p2(joint->movementUpperLimit);
		glm::vec3 c_r1(joint->rotationLowerLimit);
		glm::vec3 c_r2(joint->rotationUpperLimit);
		glm::vec3 s_p(joint->springMovementConstant);
		glm::vec3 s_r(joint->springRotationConstant);
		
		vector<float> stiffness;
		
		for (int j =  0; j < 3; ++j) stiffness.push_back(joint->springMovementConstant[j]);
		for (int j =  0; j < 3; ++j) stiffness.push_back(joint->springRotationConstant[j]);
		
		glm::vec3 p(joint->position);
		glm::vec3 r(joint->rotation);
		
		
		glm::mat4 trans, rotation, world, rot_x, rot_y, rot_z;
		trans=glm::translate(p.x, p.y, p.z);
		rotation=glm::yawPitchRoll(r.y, r.x, r.z);
		world = trans*rotation;	// ジョイントの行列（モデルローカル座標系）
		
		btRigidBody *rigidbody_a,*rigidbody_b;
		glm::mat4 a,b,frameInA,frameInB;
		
		rigidbody_a = rigidBodies[joint->relatedRigidBodyIndexA];
		a = bulletPhysics->GetWorld(rigidbody_a);	// 剛体の行列（モデルローカル座標系）
		a=glm::inverse(a);
		frameInA = a*world;
		
		if(joint->relatedRigidBodyIndexB!=-1)
		{
			rigidbody_b = rigidBodies[joint->relatedRigidBodyIndexB];
			b = bulletPhysics->GetWorld(rigidbody_b);
			b=glm::inverse(b);
			frameInB = b*world;	// ジョイントの行列（剛体ローカル座標系）
		}

		if(joint->relatedRigidBodyIndexB!=-1)
		{
			bulletPhysics->Add6DofSpringConstraint(rigidbody_a, rigidbody_b, frameInA, frameInB, c_p1, c_p2, c_r1, c_r2, stiffness);
			joint->Local=frameInA;
		}
		else
		{
			bulletPhysics->Add6DofSpringConstraint(rigidbody_a, frameInA, c_p1, c_p2, c_r1, c_r2, stiffness);
			joint->Local=frameInA;
		}
	}
	//const float length = 0.3f;
	//D3DXCreateBox(pDevice, length, length, length, &joint_mesh, 0);
}


void MMDPhysics::updateBones(bool physicsEnabled)
{
	for (unsigned int i = 0; i<pmxInfo.rigidBodies.size(); ++i)
	{		
		if(pmxInfo.rigidBodies[i]->relatedBoneIndex!=-1 && pmxInfo.rigidBodies[i]->physicsOperation == 0) // ボーン追従タイプの剛体にボーン行列を設定
		{
	
			//cout<<"here"<<endl;
			PMXBone* bone = pmxInfo.bones[pmxInfo.rigidBodies[i]->relatedBoneIndex]; //Bone* bone = &((*bones)[pmxInfo.rigidBodies[i]->relatedBoneIndex[i]]);
			const glm::mat4 rigidMat = pmxInfo.bones[pmxInfo.rigidBodies[i]->relatedBoneIndex]->calculateGlobalMatrix()*motionController->invBindPose[pmxInfo.rigidBodies[i]->relatedBoneIndex]*pmxInfo.rigidBodies[i]->Init; // ボーンの移動量を剛体の初期姿勢に適用したものが剛体の現在の姿勢			
			bulletPhysics->MoveRigidBody(rigidBodies[i], &rigidMat);
		}
	}
	
	if(physicsEnabled) bulletPhysics->StepSimulation();
	
	for (unsigned int i = 0; i < rigidBodies.size(); ++i)
	{
		if (pmxInfo.rigidBodies[i]->relatedBoneIndex!=-1 && pmxInfo.rigidBodies[i]->physicsOperation == 2) //ボーン位置あわせタイプの剛体の位置移動量にボーンの位置移動量を設定
		{
			PMXBone* bone = pmxInfo.bones[pmxInfo.rigidBodies[i]->relatedBoneIndex];
			glm::vec3 v = glm::vec3(bone->calculateGlobalMatrix()[3]) - glm::vec3(glm::inverse(motionController->invBindPose[pmxInfo.rigidBodies[i]->relatedBoneIndex])[3]);	// ボーンの位置移動量
			glm::vec3 p = glm::vec3(pmxInfo.rigidBodies[i]->Init[3])+v;	//剛体の位置
			glm::mat4 m = bulletPhysics->GetWorld(rigidBodies[i]);
			m[3][0] = p.x; m[3][1] = p.y; m[3][2] = p.z; m[3][3]=1.0;
			
			
			
			bulletPhysics->MoveRigidBody(rigidBodies[i], &m);
		}
	}
	
	for (unsigned int i = 0; i < rigidBodies.size(); ++i)
	{
		if(pmxInfo.rigidBodies[i]->relatedBoneIndex!=-1)
		{
			PMXBone* bone = pmxInfo.bones[pmxInfo.rigidBodies[i]->relatedBoneIndex];
			//bone->Global = bulletPhysics->GetWorld(rigidBodies[i])*rigidbody_offset[i]*glm::translate(bone->position);
			//bone->Global = glm::inverse(motionController->invBindPose[pmxInfo.rigidBodies[i]->relatedBoneIndex[i]]);
			
			//motionController->skinMatrix[pmxInfo.rigidBodies[i]->relatedBoneIndex] = bone->calculateGlobalMatrix()*motionController->invBindPose[pmxInfo.rigidBodies[i]->relatedBoneIndex];
			//bone->Local=bulletPhysics->GetWorld(rigidBodies[i])*pmxInfo.rigidBodies[i]->Offset*glm::translate(bone->position);
			
			//bone->Local=glm::translate(bone->position);
			
			//bone->Local = bone->Local*pmxInfo.rigidBodies[i]->Offset*bulletPhysics->GetWorld(rigidBodies[i]);
			motionController->skinMatrix[pmxInfo.rigidBodies[i]->relatedBoneIndex]=bulletPhysics->GetWorld(rigidBodies[i])*pmxInfo.rigidBodies[i]->Offset;
			
			//motionController->skinMatrix[pmxInfo.rigidBodies[i]->relatedBoneIndex]=bulletPhysics->GetWorld(rigidBodies[i])*pmxInfo.rigidBodies[i]->Offset+glm::translate(bone->position);
			//motionController->skinMatrix[pmxInfo.rigidBodies[i]->relatedBoneIndex]=bulletPhysics->GetWorld(rigidBodies[i])*pmxInfo.rigidBodies[i]->Offset*glm::translate(bone->position);
		}
	}
	
	motionController->updateBoneMatrix();
}

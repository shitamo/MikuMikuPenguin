#ifndef BULLET_PHYSICS_H
#define BULLET_PHYSICS_H

#include "btBulletDynamicsCommon.h"

#include <vector>
#include <string>
#include <fstream>

#include <glm/gtc/matrix_transform.hpp> 
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "bulletdebugdrawer.h"

class BulletPhysics
{
	public:
	BulletPhysics(GLuint shaderProgram=0);
	~BulletPhysics();
	
	///Rigid Body Creation
	///When mass is 0 and kinematic is false, it becomes a static rigid body.
	///When mass is 0 and kinematic is true, it can be moved manually, but it becomes a Kinematic rigid body that isn't effected by physics calculations
	btRigidBody* CreateBox(float width, float height, float depth, const glm::mat4* world, 
		float mass = 0, float restitution = 0, float friction = 0.5f, float linear_damp = 0, float angular_damp = 0, bool kinematic = 0, unsigned short group = 1, unsigned short mask = 0xFFFF);
	btRigidBody* CreateSphere(float radius, const glm::mat4* world, 
		float mass = 0, float restitution = 0, float friction = 0.5f, float linear_damp = 0, float angular_damp = 0, bool kinematic = 0, unsigned short group = 1, unsigned short mask = 0xFFFF);
	btRigidBody* CreateCylinder(float radius, float length, const glm::mat4* world,	//The center axis is the Z-axis
		float mass = 0, float restitution = 0, float friction = 0.5f, float linear_damp = 0, float angular_damp = 0, bool kinematic = 0, unsigned short group = 1, unsigned short mask = 0xFFFF);
	btRigidBody* CreateCapsule(float radius, float height, const glm::mat4* world,	//The center axis is the Z-axis. The height is the distance to the sphere's center.
		float mass = 0, float restitution = 0, float friction = 0.5f, float linear_damp = 0, float angular_damp = 0, bool kinematic = 0, unsigned short group = 1, unsigned short mask = 0xFFFF);
		
	void MoveRigidBody(btRigidBody* body, const glm::mat4* world);
	
	// 拘束条件追加
	void AddPointToPointConstraint(btRigidBody* body, const glm::vec3& pivot);
	void AddPointToPointConstraint(btRigidBody* bodyA, btRigidBody* bodyB, const glm::vec3& pivotInA, const glm::vec3& pivotInB);

	/// 6軸ジョイントを追加
	/// @param bodyA 剛体A
	/// @param bodyB 剛体B
	/// @param frameInA ジョイントのワールド変換行列(剛体Aローカル座標系)
	/// @param frameInB ジョイントのワールド変換行列(剛体Bローカル座標系)
	/// @param c_p1 移動制限1
	/// @param c_p2 移動制限2
	/// @param c_r1 回転制限1
	/// @param c_r2 回転制限2
	/// @param stiffness バネ剛性(平行移動x, y, z, 回転移動x, y, zの順の6要素)
	void Add6DofSpringConstraint(btRigidBody* bodyA, btRigidBody* bodyB, const glm::mat4& frameInA, const glm::mat4& frameInB,
								const glm::vec3& c_p1, const glm::vec3& c_p2, const glm::vec3& c_r1, const glm::vec3& c_r2, std::vector<float> stiffness);
	void Add6DofSpringConstraint(btRigidBody* bodyB, const glm::mat4& frameInBDX,
								const glm::vec3& c_p1, const glm::vec3& c_p2, const glm::vec3& c_r1, const glm::vec3& c_r2, std::vector<float> stiffness);
	
	//Step simulation 1/60s forward
	void StepSimulation();
	
	void setDebugMode(int mode);
	void DebugDrawWorld();

	glm::mat4 GetWorld(btRigidBody* body); //Get the resulting World Matrix
	
	
	DebugDrawer *debugDrawer;
	private:
	btDefaultCollisionConfiguration *collisionConfiguration;
	btCollisionDispatcher *dispatcher;
	btBroadphaseInterface *overlappingPairCache;
	btSequentialImpulseConstraintSolver *solver;
	btDiscreteDynamicsWorld *dynamicsWorld;
	
	//List of rigid body shapes
	btAlignedObjectArray<btCollisionShape*> collisionShapes;
	
	
	btRigidBody* CreateShape(btCollisionShape* shape, const glm::mat4* world,
	float mass, float restitution, float friction, float linear_damp, float angular_damp, bool kinematic, unsigned short group, unsigned short mask);
	
	btVector3 ConvertVectorGLToBT(const glm::vec3&);
	glm::vec3 ConvertVectorBTToGL(const btVector3&);
	btTransform ConvertMatrixGLToBT(const glm::mat4&);
	glm::mat4 ConvertMatrixBTToGL(const btTransform&);
};


#endif

#ifndef PMX_H
#define PMX_H

#include <vector>
#include <string>
#include <fstream>

#include <glm/gtc/matrix_transform.hpp> 
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>


#define WEIGHT_FORMULA_BDEF1 0
#define WEIGHT_FORMULA_BDEF2 1
#define WEIGHT_FORMULA_BDEF4 2
#define WEIGHT_FORMULA_SDEF 3

#define MORPH_TYPE_GROUP 0
#define MORPH_TYPE_VERTEX 1
#define MORPH_TYPE_BONE 2
#define MORPH_TYPE_UV 3
#define MORPH_TYPE_EXTRA_UV1 4
#define MORPH_TYPE_EXTRA_UV2 5
#define MORPH_TYPE_EXTRA_UV3 6
#define MORPH_TYPE_EXTRA_UV4 7
#define MORPH_TYPE_MATERIAL 8

#define RIGID_SHAPE_SPHERE 0
#define RIGID_SHAPE_CUBE 1
#define RIGID_SHAPE_CAPSULE 2

#define VERTEX_DEBUG false
#define MATERIAL_DEBUG true


struct PMXBone;
struct PMXInfo;

//PMX related Functions
void getPMXText(std::ifstream &miku, PMXInfo &pmxInfo, std::string &result, bool debug=false);
PMXInfo &readPMX(std::string foldername,std::string filename);

#define PMX_ENCODE_UTF16 0
#define PMX_ENCODE_UTF8 1

//PMX related structs

struct PMXVertex
{	
	glm::vec3 pos; //Vertex position
	glm::vec3 normal; //Surface normal
	glm::vec2 UV; //UV
	
	uint8_t weight_transform_formula; //0:BDEF1 1:BDEF2 2:BDEF4 3:SDEF
	
	//Bone Info
	int boneIndex1;
	int boneIndex2;
	int boneIndex3;
	int boneIndex4;
	
	float weight1;
	float weight2;
	float weight3;
	float weight4;
	
	//SDEF Bone Info
	glm::vec3 C;
	glm::vec3 R0;
	glm::vec3 R1;
	
	float edgeScale;
};

struct PMXFace
{
	unsigned points[3]; //Point indices
};

struct PMXMaterial
{
	std::string name;
	std::string nameEng;
	
	glm::vec4 diffuse;
	glm::vec3 specular;
	float shininess; //Specular Coefficient
	glm::vec3 ambient;
	
	bool drawBothSides;
	bool drawGroundShadow;
	bool drawToSelfShadowMap;
	bool drawSelfShadow;
	bool drawEdges;
	
	glm::vec4 edgeColor;
	float edgeSize;
	
	int textureIndex;
	int sphereIndex;
	
	uint8_t sphereMode; //0:no effect 1:multiplication (sph), 2:addition(spa), 3:subtexture (use extra UV1's X,Y as a UV reference for normal texture drawing)
	uint8_t shareToon; //0:Next variable is separate toon 1:Next variable is shared Toon
	
	int toonTextureIndex; //if shareToon=0, use texture at this texture index
	uint8_t shareToonTexture; //if shareToon=1, use Shared Toon Textures. Values [0~9] correspond with toon01.bmp->toon10.bmp
	
	std::string memo; //various comments
	
	int hasFaceNum; //Number of Indices that have the material (always a multiple of 3)
};

struct PMXIKLink
{
	int linkBoneIndex;
	
	bool angleLimit; //0:OFF 1:ON
	//If angleLimit==1
	glm::vec3 lowerLimit; //lowerLimit (x,y,z) -> Radian Angle
	glm::vec3 upperLimit; //upperLimit (x,y,z) -> Radian Angle
};

struct PMXBone
{
	/*Notes concerning PMXBones:
	 * When a given bone is translated/rotated, all of its children are translated/rotated
	 * 
	 * When Rotation/Movement allowed is turned on, the bone's givenParentBone's movement/rotation values get added like so:
	 * givenParentBone(x,y,z) -> thisBone(+allowRate*x, +allowRate*y, +allowRate*z)
	 * (Depending on the allowRate value, negative values also become possible)
	 * 
	 * */
	
	std::string name;
	std::string nameEng;
	
	glm::vec3 position;
	
	int parentBoneIndex; //Set to -1 when unused
	
	int transformationLevel; //used as part of determining order in which transformations are applied
	
	bool connectionDisplayMethod; //Joint display method (PMD Child Bone Selection) 0: With a coordinate offset 1: With a bone
	bool rotationPossible;
	bool movementPossible;
	bool show;
	bool controlPossible;
	bool IK; //USED IN TRANSFORMATIONS
	
	bool giveRotation; //USED IN TRANSFORMATIONS
	bool giveTranslation; //USED IN TRANSFORMATIONS
	bool axisFixed;
	bool localAxis;
	bool transformAfterPhysics; //USED IN TRANSFORMATIONS; used as part of determining order in which transformations are applied
	bool externalParentTransform; //USED IN TRANSFORMATIONS
	
	//If connectionDisplayMethod==0
	glm::vec3 coordinateOffset; //Relative to the Bone position
	
	//If connectionDisplayMethod==1
	int connectionBoneIndex;
	
	//If giveRotation==1 or giveTranslation==1
	int givenParentBoneIndex;
	float giveRate;
	
	//If axisFixed==1
	glm::vec3 axisDirectionVector;
	
	//If localAxis==1
	glm::vec3 XAxisDirectionVector;
	glm::vec3 ZAxisDirectionVector;
	
	//If externalParentTransform==1
	int keyValue;
	
	//If IK==1
	int IKTargetBoneIndex;
	unsigned IKLoopCount; //In PMD and MMD environments, this caps at 255 apparently
	float IKLoopAngleLimit; //At the time of loop calculation, angle limit each time the loop is run (Warning: 4x the value it was in PMD files)
	
	unsigned IKLinkNum; //Number of continuing IK(Inverse Kinetics) elements
	std::vector<PMXIKLink*> IKLinks;
	
	
	//VARIABLES I ADDED BELOW THIS POINT
	glm::mat4 Local; //Bone Transformation Matrix relative to the bone's parent bone.
	
	PMXBone *parent;
	
	glm::mat4 calculateGlobalMatrix()
	{
		if(parent) return parent->calculateGlobalMatrix() * Local;
		else return Local;
	}
};

struct PMXMorphData
{
	//Only inherited classes used
};

struct PMXVertexMorph: PMXMorphData
{
	int vertexIndex;
	glm::vec3 coordinateOffset;
};

struct PMXUVMorph: PMXMorphData
{
	int vertexIndex;
	glm::vec4 UVOffsetAmount; //For normal UVs, z and w become unneeded data, but still get stored as part of morph data
};

struct PMXBoneMorph: PMXMorphData
{
	int boneIndex;
	glm::vec3 inertia; //interia
	glm::vec4 rotationAmount; //Quarternion (x,y,z,w)
};

struct PMXMaterialMorph: PMXMorphData
{
	int materialIndex; //-1: For all materials
	uint8_t offsetCalculationFormula; //0:Multiplication 1:Addition
	
	glm::vec4 diffuse; //Default values are: Multiplication: 1.0, Addition: 0.0 (same for below variables)
	
	glm::vec3 specular;
	float shininess; //Specular coefficient
	
	glm::vec3 ambient;
	
	glm::vec4 edgeColor;
	float edgeSize;
	
	glm::vec4 textureCoefficient; //(R,G,B,A)
	glm::vec4 sphereTextureCoefficient;
	glm::vec4 toonTextureCoefficient;
};

struct PMXGroupMorph: PMXMorphData
{
	int morphIndex; //Note: Group morphs can not be made into group morphs
	float morphRate; //Group morph's morph points * morphRate = target morph's morph points
};

struct PMXMorph
{
	std::string name;
	std::string nameEng;
	
	uint8_t controlPanel; //(PMD:Category) 1:Eyebrow(bottom left) 2:Eye(upper left) 4:Other(bottom right) 0:System reserved
	uint8_t type; //Morph type 0:Group 1:Vertex 2:Bone 3:UV 4:Extra UV1, 5:Extra UV 2, 6:Extra UV 3, 7:Extra UV4, 8:Material
	
	int morphOffsetNum; //Number of continuing offset data
	
	std::vector<PMXMorphData*> offsetData;
};

struct PMXDisplayFrameElement
{
	bool target; //0: Bone 1:Morph
	unsigned int index;
};

struct PMXDisplayFrame
{
	std::string name;
	std::string nameEng;
	
	bool specialFrameFlag; //0:Normal Frame 1:Special Frame
	int elementsWithinFrame; //Number of continuing elements
	
	std::vector<PMXDisplayFrameElement*> elements;
};

struct PMXRigidBody
{
	std::string name;
	std::string nameEng;
	
	int relatedBoneIndex; //Set to -1 when irrelevant/unrelated
	
	uint8_t group;
	unsigned short noCollisionGroupFlag;
	
	uint8_t shape; //0:Circle 1:Square 2:Capsule
	glm::vec3 size; //(x,y,z)
	
	glm::vec3 position;
	glm::vec3 rotation; //->radian angle
	
	float mass;
	float movementDecay; //movement reduction
	float rotationDecay; //rotation reduction
	float elasticity; //recoil
	float friction; //strength of friction
	
	uint8_t physicsOperation; //0:Follow Bone (static), 1:Physics Calc. (dynamic), 2: Physics Calc. + Bone position matching
	
	
	//VARIABLES I ADDED BELOW THIS POINT
	glm::mat4 Init; //The initial transformation matrix
	glm::mat4 Offset; //The current offset matrix
};

struct PMXJoint
{
	std::string name;
	std::string nameEng;
	
	bool type; //0:Spring 6DOF; in PMX 2.0 always set to 0 (included to give room for expansion)
	
	//If joint type==0
	int relatedRigidBodyIndexA; //-1 if irrelevant
	int relatedRigidBodyIndexB;
	
	//Spring position/rotation
	glm::vec3 position;
	glm::vec3 rotation;
	
	glm::vec3 movementLowerLimit;
	glm::vec3 movementUpperLimit;
	glm::vec3 rotationLowerLimit; //-> radian angle
	glm::vec3 rotationUpperLimit; //-> radian angle
	
	glm::vec3 springMovementConstant;
	glm::vec3 springRotationConstant;
	
	//VARIABLES I ADDED BELOW THIS POINT
	glm::mat4 Local; //joint matrix, local to relatedRigidBodyA
};

struct PMXInfo
{
	//For more info, see the PMX documentation included with PMDEditor (Hopefully also included with this source code)
	//also http://gulshan-i-raz.geo.jp/labs/2012/10/17/pmx-format1/
	
	//Header
	char header_str[4]; //Should read 'PMX ' for 2.0+
	float ver; //version number
	
	uint8_t line_size; //number of bytes per line
	bool unicode_type;
	
	uint8_t extraUVCount;
	
	uint8_t vertexIndexSize;
	uint8_t textureIndexSize;
	uint8_t materialIndexSize;
	uint8_t boneIndexSize;
	uint8_t morphIndexSize;
	uint8_t rigidBodyIndexSize;
	
	//Model Info
	std::string modelName;
	std::string modelNameEnglish;
	std::string comment;
	std::string commentEnglish;
	
	//Vertex
	int vertex_continuing_datasets; //Number of continuing Vertex Datasets
	std::vector<PMXVertex*> vertices;
	
	//Face
	int face_continuing_datasets;
	std::vector<PMXFace*> faces;
	
	//Texture
	int texture_continuing_datasets;
	std::string *texturePaths;
	
	//Material
	int material_continuing_datasets;
	std::vector<PMXMaterial*> materials;
	
	//Bone
	int bone_continuing_datasets;
	std::vector<PMXBone*> bones;
	
	//Morph (Emotion data)
	int morph_continuing_datasets;
	std::vector<PMXMorph*> morphs;
	
	//Display Frame
	int display_frame_continuing_datasets;
	std::vector<PMXDisplayFrame*> displayFrames;
	
	//Rigid Body
	int rigid_body_continuing_datasets;
	std::vector<PMXRigidBody*> rigidBodies;
	
	//Joint
	int joint_continuing_datasets;
	std::vector<PMXJoint*> joints;
};

#endif

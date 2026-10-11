#ifndef MOTION_CONTROLLER_H
#define MOTION_CONTROLLER_H

#include "mmp_gl.h"

//Texture unit used for the bone matrix buffer texture (units 0-2 are the
//material, sphere and toon textures).
#define BONE_TEXTURE_UNIT 3
#include "pmx.h"
#include "vmd.h"

#include <list>
#include <sstream>

extern glm::quat IKQuat;

struct VertexData;

/*!  \class VMDMotionController
 * \if ENGLISH
 * \brief Class for animating a PMX model using motion data from a VMD file.
 * 
 * Detailed description starts here(VMDMotionController).
 * \endif
 * 
 * \if JAPANESE
 * \brief VMDモーションデーターでPMXモデルを動かす為のクラス。
 * 
 * 詳細はここに（VMDMotionController）。
 * \endif
*/
class VMDMotionController
{
	public:
	/*! \if ENGLISH \brief Constructs a VMDMotionController instance.
	 * 
	 * @param pmxInfo The PMX Model to be animated.
	 * @param vmdInfo The VMD Motion to animate the model with.
	 * @param shaderProgram The shader program from which to pull the location for the uniform variable 'Bones' from.
	 * \endif
	 * \if JAPANESE \brief VMDMotionControllerのコンストラクタ。
	 * 
	 * @param pmxInfo 動かされるPMXモデル。
	 * @param vmdInfo モデルを動かすVMDモーション。
	 * @param shaderProgram Uniform変数「Bones」を取得出来るシェーダープログラム。
	 * \endif
	*/
	VMDMotionController(ClosedMMDFormat::PMXInfo &pmxInfo,ClosedMMDFormat::VMDInfo &vmdInfo,GLuint shaderProgram);			
	~VMDMotionController();
	void updateVertexMorphs(); 			//!< \if ENGLISH \brief A function used to update the Vertices of a model based on Vertex Morph animation parameters. \endif
										//!< \if JAPANESE \brief 頂点モーフのモーションパラメータを利用してモデルの頂点を更新する為の関数。 \endif
	void updateBoneMatrix();
	void bindBoneTexture();				//!< bind this model's bone matrices to BONE_TEXTURE_UNIT (when several models are drawn)			//!< \if ENGLISH \brief A function used to update the uniform 'Bones' in the shader program shaderProgram with the model's bone transformation matrices. \endif
										//!< \if JAPANESE \brief Uniform変数「Bones」をモデルのボーン変形行列の配列で更新する為の関数。 \endif
	void updateBoneAnimation();			//!< \if ENGLISH \brief A function used to update the model's bone transformation matrices with the current frame of animation. \endif
										//!< \if JAPANESE \brief モデルのボーン変形行列の配列を更新する為の関数。 \endif
	/*!
	 * \if ENGLISH
	 * \brief A function used to move on to the next frame in the animation.
	 * \return true if animation is finished
	 * \endif
	 * \if JAPANESE
	 * \brief モーションの次のフレームへと進む為の関数。
	 * \return アニメーションが終了したらtrue
	 * \endif
	 */
	bool advanceTime();
	
	
	//public variables
	glm::mat4 *skinMatrix; 				//!< \if ENGLISH \brief A pointer to an array of bone transformation matrices that represent the model's current pose. \endif
										//!< \if JAPANESE \brief モデルの現在ポーズを表すボーン変形行列の配列へのポインター。 \endif
	glm::mat4 *invBindPose;				//!< \if ENGLISH \brief A pointer to an array of bone transformation matrices that represent the inverse of the model's initial pose. \endif
										//!< \if JAPANESE \brief モデルの初期ポーズの逆性を表すボーン変形行列の配列へのポインター。 \endif
	VertexData *vertexData;
	
	//! Per-material values after applying material morphs (same order as pmxInfo.materials)
	struct MaterialState
	{
		glm::vec4 diffuse;
		glm::vec3 specular;
		float shininess;
		glm::vec3 ambient;
		glm::vec4 edgeColor;
		float edgeSize;
		glm::vec4 textureMul; //texture colour = texture*textureMul + textureAdd
		glm::vec4 textureAdd;
		glm::vec4 sphereMul;  //same for the sphere map
		glm::vec4 sphereAdd;
		glm::vec4 toonMul;    //same for the toon texture
		glm::vec4 toonAdd;
	};
	std::vector<MaterialState> materialStates;
	
	//! Effective weight of every morph this frame (VMD value + group morph contributions)
	std::vector<float> morphWeights;
	
	//! Additional UVs after UV morphs: extraUVs[vertex*4 + k] (only filled when the model has extra UVs).
	//! The viewer does not render them; they are provided for library users.
	std::vector<glm::vec4> extraUVs;
	
	//! Per-bone offset from bone morphs this frame (applied on top of the VMD pose)
	std::vector<glm::vec3> boneMorphPos;
	std::vector<glm::quat> boneMorphRot;				//!< \if ENGLISH \brief A pointer to the array of vertices that the model is made of. \endif
										//!< \if JAPANESE \brief モデルが出来ている頂点の配列へのポインター。 \endif
	
	
	//! Current animation time in 1/60 s steps (VMD frame = time/2)
	int getTime() const { return time; }
	
	/*! Jump to time t (1/60 s steps, VMD frame = t/2).  Call
	 *  updateVertexMorphs() and updateBoneAnimation() afterwards (and reset
	 *  the physics) to show the pose of that time. */
	void setTime(int t);
	//! Frame (30 fps) of the last bone or morph key of the motion
	unsigned lastKeyFrame() const;
	
	//! Model visibility from the VMD "show/IK" frames ("表示") at the current time
	bool isModelVisible() const;
	//! Whether the IK of bone boneIndex is enabled by the VMD "show/IK" frames at the current time
	bool isIKEnabled(int boneIndex) const;
	
	private:
	int time;
	
	void updateChildren(ClosedMMDFormat::PMXBone *linkBone);
	void updateIK();
	void updateInheritance(); //PMX rotation/translation inheritance ("付与")
	void updateMaterialMorphs();
	glm::vec3 animatedPosition(int boneIndex) const;
	glm::quat animatedRotation(int boneIndex) const;
	void applyInheritance(int boneIndex, std::vector<char> &state);
	void solveIKBone(int boneIndex);
	std::vector<int> boneOrder; //bones in deformation order (level, index)
	void solveIK(ClosedMMDFormat::PMXBone *joint, int effector, glm::vec3 targetPos, int numIteration, int numBones, int numMaxBones);
	
	
	ClosedMMDFormat::PMXInfo &pmxInfo;
	ClosedMMDFormat::VMDInfo &vmdInfo;
	
	private:
	GLuint Bones_loc; //uniform location of the boneTexture sampler in the shader
	GLuint boneBuffer; //GL_TEXTURE_BUFFER holding skinMatrix[]
	GLuint vertexBuffer; //model vertex buffer (GL_ARRAY_BUFFER bound at construction)
	GLuint boneTexture; //buffer texture view of boneBuffer
	
	//***BONE TRANSFORMATION VARIABLES
	std::vector<std::list<ClosedMMDFormat::VMDBoneFrame>> boneKeyFrames;				//Hold a list of keyframes for each Bone
	std::vector<std::list<ClosedMMDFormat::VMDBoneFrame>::iterator> ite_boneKeyFrames;	//Keyframe iterator
	
	std::vector<glm::quat> boneRot;
	std::vector<glm::vec3> bonePos;	
	
	//***VERTEX MORPH VARIABLES
	std::vector<std::list<ClosedMMDFormat::VMDMorphFrame>> morphKeyFrames;				//Hold a list of keyframes for each vertex morph
	std::vector<std::list<ClosedMMDFormat::VMDMorphFrame>::iterator> ite_morphKeyFrames;	//Keyframe iterator
	
	std::vector<float> vMorphWeights;
	
	//***SHOW/IK FRAMES (step keys: frame, value)
	std::vector<std::vector<std::pair<unsigned,bool>>> ikEnableKeys; //per bone (only IK bones named in the VMD)
	std::vector<std::pair<unsigned,bool>> visibleKeys;
};

/*! \struct VertexData
 * \if ENGLISH \brief Struct for holding MMD Model Vertex Information. \endif
 * \if JAPANESE \brief MMDモデルの頂点情報を持つ為の構造体。 \endif　*/
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

	//SDEF parameters (only used when weightFormula==3).  sdefR0/sdefR1 hold
	//the pre-processed "(C + r0)/2" / "(C + r1)/2" points.
	glm::vec3 sdefC;
	glm::vec3 sdefR0;
	glm::vec3 sdefR1;

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



#endif

#include "motioncontroller.h"

#include <sstream>
#include <iostream>

#define GLM_FORCE_RADIANS

#define M_PI 3.14159265358979323846

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/euler_angles.hpp>

//#define MODELDUMP true

using namespace std;

float ipfunc(float t, float p1, float p2);
float ipfuncd(float t, float p1, float p2);
float bezierp(float x1, float x2, float y1, float y2, float x);

glm::fquat Slerp(glm::quat &v0, glm::quat &v1, float alpha);
glm::vec4 Vectorize(const glm::quat theQuat);

glm::vec4 Lerp(const glm::vec4 &start, const glm::vec4 &end, float alpha);
glm::vec4 Slerp(glm::vec4 &v0, glm::vec4 &v1, float alpha);

float Lerp(const float &start, const float &end, float alpha);
float Slerp(float &v0, float &v1, float alpha);

glm::quat IKQuat;

VMDMotionController::VMDMotionController(PMXInfo &pmxInfovar,VMDInfo
&vmdInfovar,GLuint shaderProgram):pmxInfo(pmxInfovar),vmdInfo(vmdInfovar)
{
	//***INIT BONE TRANSFORMATION VARIABLES***
	time=0;

	invBindPose=new glm::mat4[pmxInfo.bone_continuing_datasets];
	for(int i=0; i<pmxInfo.bone_continuing_datasets; ++i)
	{
		PMXBone *b = pmxInfo.bones[i];
		//glm::vec3 DXToGLPosition(b->position.x,b->position.y,-b->position.z);
		invBindPose[i] = glm::translate( -b->position );
	}
	skinMatrix=new glm::mat4[pmxInfo.bone_continuing_datasets]();

	Bones_loc=glGetUniformLocation(shaderProgram,"Bones");


	boneKeyFrames.resize(pmxInfo.bone_continuing_datasets);
	for(unsigned i=0; i<vmdInfo.boneCount; ++i)
	{
		for(unsigned j=0; j<pmxInfo.bone_continuing_datasets; ++j)
		{
			//Search for the bone number from the bone name
			if(vmdInfo.boneFrames[i].name == pmxInfo.bones[j]->name)
			{
				boneKeyFrames[j].push_back(vmdInfo.boneFrames[i]);
				break;
			}
		}
	}

	for(unsigned int i=0; i<pmxInfo.bone_continuing_datasets; ++i)
	{
		boneKeyFrames[i].sort();
		ite_boneKeyFrames.push_back(boneKeyFrames[i].begin());
		boneRot.push_back(glm::quat(0, 0, 0, 0));
		boneRot[boneRot.size()-1].w=1;
		bonePos.push_back(glm::vec3(0, 0, 0));
	}

#ifdef MODELDUMP
	ofstream modeldump("modeldump.txt");
#endif

	//***INIT VERTEX DATA VARIABLES***
	vertexData =
(VertexData*)malloc(pmxInfo.vertex_continuing_datasets*sizeof(VertexData));
	for(int i=0; i<pmxInfo.vertex_continuing_datasets; ++i)
	{
		vertexData[i].position.x=pmxInfo.vertices[i]->pos[0];
		vertexData[i].position.y=pmxInfo.vertices[i]->pos[1];
		vertexData[i].position.z=pmxInfo.vertices[i]->pos[2];
		vertexData[i].position.w=1.0;

		vertexData[i].UV.x=pmxInfo.vertices[i]->UV[0];
		vertexData[i].UV.y=pmxInfo.vertices[i]->UV[1];

		vertexData[i].normal.x=pmxInfo.vertices[i]->normal[0];
		vertexData[i].normal.y=pmxInfo.vertices[i]->normal[1];
		vertexData[i].normal.z=pmxInfo.vertices[i]->normal[2];

		vertexData[i].weightFormula=pmxInfo.vertices[i]->weight_transform_formula;


		vertexData[i].boneIndex1=pmxInfo.vertices[i]->boneIndex1;
		vertexData[i].boneIndex2=pmxInfo.vertices[i]->boneIndex2;
		vertexData[i].boneIndex3=pmxInfo.vertices[i]->boneIndex3;
		vertexData[i].boneIndex4=pmxInfo.vertices[i]->boneIndex4;

		vertexData[i].weight1=pmxInfo.vertices[i]->weight1;
		vertexData[i].weight2=pmxInfo.vertices[i]->weight2;
		vertexData[i].weight3=pmxInfo.vertices[i]->weight3;
		vertexData[i].weight4=pmxInfo.vertices[i]->weight4;

		//cout<<vertexData[i].position.x<<" "<<vertexData[i].position.y<<"
"<<vertexData[i].position.z<<" "<<vertexData[i].position.w<<endl;
		//cout<<vertexData[i].UV.x<<" "<<vertexData[i].UV.y<<endl;

		/*if(pmxInfo.vertices[i]->weight_transform_formula>2)
		{
			cerr<<"SDEF and QDEF not supported yet"<<endl;
			exit(EXIT_FAILURE);
		}*/

		#ifdef MODELDUMP
		modeldump << vertexData[i].str();
		#endif
	}

	// Ëø
ä†
	glBufferData(GL_ARRAY_BUFFER,
pmxInfo.vertex_continuing_datasets*sizeof(VertexData), vertexData,
GL_DYNAMIC_DRAW);

	morphKeyFrames.resize(pmxInfo.morph_continuing_datasets);
	for(unsigned i=0; i<vmdInfo.morphCount; ++i)
	{
		for(unsigned j=0; j<pmxInfo.morph_continuing_datasets; ++j)
		{
			//Search for the bone number from the bone name
			if(vmdInfo.morphFrames[i].name == pmxInfo.morphs[j]->name)
			{
				morphKeyFrames[j].push_back(vmdInfo.morphFrames[i]);
				break;
			}
		}
	}

	for(unsigned i=0; i<pmxInfo.morph_continuing_datasets; ++i)
	{
		morphKeyFrames[i].sort();
		ite_morphKeyFrames.push_back(morphKeyFrames[i].begin());

		vMorphWeights.push_back(0);
	}

	//***Setup Buffers***

}
VMDMotionController::~VMDMotionController()
{
	free(invBindPose);
	free(skinMatrix);
	//free(vertexData);
}



void VMDMotionController::updateVertexMorphs()
{
	for(unsigned i=0; i<pmxInfo.vertex_continuing_datasets; i++)
	{
		vertexData[i].position.x=pmxInfo.vertices[i]->pos.x;
		vertexData[i].position.y=pmxInfo.vertices[i]->pos.y;
		vertexData[i].position.z=pmxInfo.vertices[i]->pos.z;
	}

	for(unsigned i=0; i<pmxInfo.morph_continuing_datasets; ++i)
	{
		PMXMorph *morph=pmxInfo.morphs[i];

		if(morph->type==MORPH_TYPE_VERTEX)
		{
			unsigned long t0,t1;
			float val0,val1; //morph values to interpolate between
			glm::vec3 p0,p1;

			float ipolValue=0.0f; //interpolated value

			if(ite_morphKeyFrames[i] != morphKeyFrames[i].end())
			{
				t0=(*ite_morphKeyFrames[i]).frame*2; //MMD=30fps, this
program=60fps, 60/30=2
				val0= (*ite_morphKeyFrames[i]).value;

				//cout<<morph->name<<" "<<(*ite_morphKeyFrames[i]).name<<val0<<endl;

				if(++ite_morphKeyFrames[i] != morphKeyFrames[i].end())
				{
					t1 = (*ite_morphKeyFrames[i]).frame*2;
					val1 = (*ite_morphKeyFrames[i]).value;

					float s = (float)(time - t0)/(float)(t1 - t0);  //Linear
Interpolation


					//ipolValue=val0 + (val1 - val0)*s;
					vMorphWeights[i]=val0 + (val1 - val0)*s;

					if(time!=t1) --ite_morphKeyFrames[i];
				}
			}

			//cout<<ipolValue<<endl;

			for(int j=0; j<morph->morphOffsetNum; ++j)
			{
				PMXVertexMorph *vMorph=(PMXVertexMorph*) morph->offsetData[j];
				glm::vec3 &morphTarget=vMorph->coordinateOffset;

				glm::vec3
diffVector=pmxInfo.vertices[vMorph->vertexIndex]->pos+morphTarget-pmxInfo.ve
rtices[vMorph->vertexIndex]->pos;

				vertexData[vMorph->vertexIndex].position.x+=diffVector.x *
vMorphWeights[i];
				vertexData[vMorph->vertexIndex].position.y+=diffVector.y *
vMorphWeights[i];
				vertexData[vMorph->vertexIndex].position.z+=diffVector.z *
vMorphWeights[i];
				//vertexData[vMorph->vertexIndex].position.w=vertexPosition.w;
			}

		}
	}
	glBufferData(GL_ARRAY_BUFFER,
pmxInfo.vertex_continuing_datasets*sizeof(VertexData), vertexData,
GL_DYNAMIC_DRAW);
}



glm::vec3 convertVectorDXToGL(const glm::vec3& v)
{
	return glm::vec3(v.x, v.y, v.z);
}
glm::vec3 convertVectorGLToDX(const glm::vec3& v)
{
	return glm::vec3(v.x, v.y, v.z);
}

glm::vec4 convertVectorDXToGL(const glm::vec4& v)
{
	return glm::vec4(v.x, v.y, -v.z, v.w);
}
glm::vec4 convertVectorGLToDX(const glm::vec4& v)
{
	return glm::vec4(v.x, v.y, -v.z, v.w);
}

glm::quat convertVectorDXToGL(const glm::quat& v)
{
	return glm::quat(v.w, v.x, v.y, -v.z);
}
glm::quat convertVectorGLToDX(const glm::quat& v)
{
	return glm::quat(v.w, v.x, v.y, -v.z);
}


glm::mat4 convertMatrixDXToGL(const glm::mat4& m)
{
	glm::mat4 basis(
		m[0][0], m[1][0], -m[2][0], 0.f,
		m[0][2], m[1][1], -m[2][1], 0.f,
		-m[0][3], -m[1][2], m[2][2], 0.f,
		0.0f, 0.0f, 0.0f, 1.0f
		 );
	return basis;// * glm::vec4(m[4][1], m[4][2], m[4][3], 1.0f);
	//return btTransform(basis, btVector3(m._41, m._42, -m._43));
}

glm::mat4 convertMatrixGLToDX(const glm::mat4& t)
{
	glm::vec4 R = t[0];
	glm::vec4 U = t[1];
	glm::vec4 L = t[2];
	glm::vec4 P = t[3];
	glm::mat4 m(
		R.x, R.y, -R.z, R.w,
		U.x, U.y, -U.z, U.w,
		-L.x, -L.y, L.z, L.w,
		P.x, P.y, -P.z, P.w );
	return m;
}



void VMDMotionController::updateBoneMatrix()
{
	PMXBone   *b  = pmxInfo.bones[0];

	//Root+FKBones
	for(unsigned i = 0; i < pmxInfo.bone_continuing_datasets; i++)
	{
		b  = pmxInfo.bones[i];

		unsigned long t0,t1;
		glm::quat q0,q1;
		glm::vec3 p0,p1;

		boneRot[i]=glm::quat(1.0f,0.0f,0.0f,0.0f);
		bonePos[i]=glm::vec3(0.0f,0.0f,0.0f);

		if(ite_boneKeyFrames[i] != boneKeyFrames[i].end())
		{
			t0=(*ite_boneKeyFrames[i]).frame*2; //MMD=30fps, this program=60fps,
60/30=2
			boneRot[i] = q0 = (*ite_boneKeyFrames[i]).rotation;
			bonePos[i] = p0 = (*ite_boneKeyFrames[i]).translation;

			//BezierParameters &bezier=(*ite_boneKeyFrames[i]).bezier;

			if(++ite_boneKeyFrames[i] != boneKeyFrames[i].end())
			{
				t1 = (*ite_boneKeyFrames[i]).frame*2;
				q1 = (*ite_boneKeyFrames[i]).rotation;
				p1 = (*ite_boneKeyFrames[i]).translation;

				float s = (float)(time - t0)/(float)(t1 - t0);  //Linear
Interpolation

				boneRot[i]=Slerp(q0,q1,s);
				bonePos[i]=p0 + (p1 - p0)*s;

				if(time!=t1) --ite_boneKeyFrames[i];
			}
		}

		if(b->parentBoneIndex!=-1)
		{
			PMXBone *parent = pmxInfo.bones[b->parentBoneIndex];
			b->Local = glm::translate( b->position - parent->position +
bonePos[i] ) * glm::toMat4(boneRot[i]);
			b->Global = parent->Global * b->Local;
			skinMatrix[i] = b->Global * invBindPose[i];
		}
		else
		{
			b->Local = glm::translate( b->position + bonePos[i] ) *
glm::toMat4(boneRot[i]);
			b->Global = b->Local;
			skinMatrix[i] = b->Global * invBindPose[i];
		}
	}

	updateIK();


	glUniformMatrix4fv(Bones_loc, pmxInfo.bone_continuing_datasets, GL_FALSE,
(const GLfloat*)skinMatrix);
}



void VMDMotionController::updateChildren(PMXBone *linkBone)
{
	//cout<<linkBone->name<<" "<<linkBone->children.size()<<endl;
	for(int i=0; i<linkBone->children.size(); ++i)
	{
		PMXBone *bone = linkBone->children[i];

		int index = bone->boneIndex;
		PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
		glm::mat4 thisRotation=glm::toMat4(boneRot[bone->boneIndex]);
		glm::mat4 thisTranslation=glm::translate( bonePos[bone->boneIndex] +
bone->position - parent->position );
		bone->Local = thisTranslation * thisRotation;
		bone->Global = parent->Global * bone->Local;
		skinMatrix[bone->boneIndex] = bone->Global *
invBindPose[bone->boneIndex];

		updateChildren(bone);
	}
}



void Vector3Transform( glm::vec3 *pvec3Out, const glm::vec3 *pVec3In, const
glm::mat4 matTransform )
{
	glm::vec3 vec3Temp;

	vec3Temp.x = pVec3In->x * matTransform[0][0] + pVec3In->y *
matTransform[1][0] + pVec3In->z * matTransform[2][0] + matTransform[3][0];
	vec3Temp.y = pVec3In->x * matTransform[0][1] + pVec3In->y *
matTransform[1][1] + pVec3In->z * matTransform[2][1] + matTransform[3][1];
	vec3Temp.z = pVec3In->x * matTransform[0][2] + pVec3In->y *
matTransform[1][2] + pVec3In->z * matTransform[2][2] + matTransform[3][2];

	*pvec3Out = vec3Temp;
}

void limitAngle(glm::quat &pvec4Out, const glm::quat &pvec4Src, PMXIKLink
*link);


bool FactoringQuaternionXYZ(glm::quat quat, float &XRot, float &YRot, float
&ZRot)
{
	glm::mat3 input = glm::toMat3(quat);
	//YËª∏Âõû„Çä
ÆÂõûËª
ÇíÂè
æó
	if (input[0][2] > 1 - 1.0e-4 || input[0][2] < -1 + 1.0e-4)
	{
		//„Ç∏„É≥„Éê
´„É≠„É
ÇØÂà
Æö
		XRot = 0;
		YRot = (input[0][2] < 0 ? (float) M_PI/2 : (float) -M_PI/2);
		ZRot = -(float) atan2f(-input[1][0], input[1][1]);
		return false;
	}
	YRot = -asinf(input[0][2]);
	//XËª∏Âõû„Çä
ÆÂõûËª
ÇíÂè
æó
	XRot = asinf(input[1][2]/cosf(YRot));
	if(isnan(XRot))
	{
		//„Ç∏„É≥„Éê„É´„É≠
É„ÇØÂà§ÂÆö
ºè„ÇåÂ
Á≠

		XRot = 0;
		YRot = (input[0][2] < 0 ? (float) M_PI/2 : -(float) M_PI/2);
		ZRot = -atan2f(-input[1][0], input[1][1]);
		return false;
	}
	if(input[2][2] < 0)
	XRot = (float) M_PI - XRot;
	//ZËª∏Âõû„Çä
ÆÂõûËª
ÇíÂè
æó
	ZRot = atan2f(input[0][1], input[0][0]);
	return true;
}

bool FactoringQuaternionYZX(glm::quat quat, float &YRot, float &ZRot, float
&XRot)
{
	glm::mat3 rot = glm::toMat3(quat);
	//ZËª∏Âõû„Çä
ÆÂõûËª
ÇíÂè
æó
	if(rot[1][0] > 1 - 1.0e-4 || rot[1][0] < -1 + 1.0e-4)
	{
		//„Ç∏„É≥„Éê
´„É≠„É
ÇØÂà
Æö
		YRot = 0;
		ZRot = (rot[1][0] < 0 ? M_PI/2 : -M_PI/2);
		XRot = -atan2f(-rot[2][1], rot[2][2]);
		return false;
	}
	ZRot = -asinf(rot[1][0]);
	//YËª∏Âõû„Çä
ÆÂõûËª
ÇíÂè
æó
	YRot = asinf(rot[2][0]/cosf(ZRot));
	if(isnan(YRot))
	{
		//„Ç∏„É≥„Éê„É´„É≠
É„ÇØÂà§ÂÆö
ºè„ÇåÂ
Á≠

		YRot = 0;
		ZRot = (rot[1][0] < 0 ? M_PI/2 : -M_PI/2);
		XRot = -atan2f(-rot[2][1], rot[2][2]);
		return false;
	}
	if (rot[0][0] < 0)
		YRot = M_PI - YRot;
	//XËª∏Âõû„Çä
ÆÂõûËª
ÇíÂè
æó
	XRot = atan2f(rot[1][2], rot[1][1]);
	return true;
}

bool FactoringQuaternionZXY(glm::quat input, float &ZRot, float &XRot, float
&YRot)
{
	glm::mat3 rot = glm::toMat3(input);
	//„É®„Éº(XËª∏Âë®
ä„ÅÆÂõûË
)„Çí
ñÂ

	if (rot[2][1] > 1 - 1.0e-4 || rot[2][1] < -1 + 1.0e-4)
	{
		//„Ç∏„É≥„Éê
´„É≠„É
ÇØÂà
Æö
		XRot = (rot[2][1] < 0 ? M_PI/2 : -M_PI/2);
		ZRot = 0;
		YRot = (float) atan2f(-rot[0][2], rot[0][0]);
		return false;
	}
	XRot = -(float) asinf(rot[2][1]);
	//„É≠„Éº
´„Çí
ñÂ

	ZRot = (float) asinf(rot[0][1]/cosf(XRot));
	if (isnan(ZRot))
	{
		//Êºè„
ÂØ
≠ñ
		XRot = (rot[2][1] < 0 ? M_PI/2 : -M_PI/2);
		ZRot = 0;
		YRot = (float) atan2f(-rot[0][2], rot[0][0]);
		return false;
	}
	if (rot[1][1] < 0)
		ZRot = M_PI - ZRot;
	//„Éî„ÉÉ
Å„Çí
ñÂ

	YRot = (float) atan2f(rot[2][0], rot[2][2]);
	return true;
}


/*void VMDMotionController::updateIK()
{
	glm::vec4 localEffectorPos(0,0,0,0);
	glm::vec4 localTargetPos(0,0,0,0);

	for(int b=0; b<pmxInfo.bone_continuing_datasets; ++b)
	{
		PMXBone *IKBone=pmxInfo.bones[b];

		if(IKBone->IK) //IK„Éï„É©„Ç
Çí„ÉÅ„
„É
ÇØ
		{
			PMXBone *targetBone=pmxInfo.bones[IKBone->IKTargetBoneIndex];

			glm::vec3 targetPos;
			targetPos.x=IKBone->Global[3][0];
			targetPos.y=IKBone->Global[3][1];
			targetPos.z=IKBone->Global[3][2];

			for( int iterations = 0 ; iterations < IKBone->IKLoopCount ;
iterations++ )
			{
				for( int attentionIndex = 0 ; attentionIndex < IKBone->IKLinkNum ;
attentionIndex++ )
				{
					PMXIKLink *IKLink=IKBone->IKLinks[attentionIndex];
					PMXBone *linkBone=pmxInfo.bones[IKLink->linkBoneIndex];

					glm::vec3 effectorPos; // =
Vector3.TransformCoordinate(ikBone.IkTargetBone.Position,
ikBone.IkTargetBone.GrobalPose);
					effectorPos.x=targetBone->Global[3][0];
					effectorPos.y=targetBone->Global[3][1];
					effectorPos.z=targetBone->Global[3][2];

					glm::vec3 linkPos; // =
Vector3.TransformCoordinate(ikLink.ikLinkBone.Position,
ikLink.ikLinkBone.GrobalPose);
					linkPos.x=linkBone->Global[3][0];
					linkPos.y=linkBone->Global[3][1];
					linkPos.z=linkBone->Global[3][2];

					glm::vec3 link2Effector = glm::normalize(effectorPos - linkPos);
					glm::vec3 link2Target = glm::normalize(targetPos - linkPos);

					//ÂõûËª¢Ëß
∫¶„ÇíÊ
„Ç
Çã
					float dot = glm::dot(link2Effector, link2Target);
					float rotationAngle = acosf(dot);
					////ËßíÂ∫¶Âà∂
ê„ÇíÈÅ
î®„Å
Çã
					//if (rotationAngle > IKBone->IKLoopAngleLimit) rotationAngle =
IKBone->IKLoopAngleLimit;
					//if (rotationAngle < -IKBone->IKLoopAngleLimit) rotationAngle
= -IKBone->IKLoopAngleLimit;

					glm::vec3 rotationAxis = glm::cross(link2Effector, link2Target);

					if(isnan(rotationAngle)==false && rotationAngle > 1.0e-8f)
					{
						//cout<<"here"<<endl;
						//Ëª∏„Çí‰∏≠ÂøÉ„Å®„Åó„
ÂõûËª¢„Åô„Çã
åÂàó„Çí‰
Êàê„
„Ç
ÄÇ
						glm::mat4 rotMatrix = glm::rotate(rotationAngle, rotationAxis);
						glm::quat rotation = glm::toQuat(rotMatrix);
						//ikLink.ikLinkBone.Rotation = rotation *
ikLink.ikLinkBone.Rotation;


						glm::vec3 euler = glm::eulerAngles(rotation) * 3.14159f / 180.f;

							euler.x = glm::clamp(euler.x, IKLink->lowerLimit.x,
IKLink->upperLimit.x);
							euler.y = glm::clamp(euler.y, IKLink->lowerLimit.y,
IKLink->upperLimit.y);
							euler.z = glm::clamp(euler.z, IKLink->lowerLimit.z,
IKLink->upperLimit.z);

							cout<<euler.x<<" "<<IKLink->lowerLimit.x<<"
"<<IKLink->upperLimit.x<<endl;

							//rotation = glm::toQuat( glm::orientate3(euler) );
							rotation = glm::toQuat( glm::eulerAngleX(euler.x) *
glm::eulerAngleY(euler.y) * glm::eulerAngleZ(euler.z) );

						//ÂõûËª
áèÂà
ôê
						if(IKLink->angleLimit)
						{




							float xRotation, yRotation, zRotation;//X,Y,ZËª∏ÂõûËª
¢Èáè„Å´Â
Êèõ„
„Ç
ÄÇ
							int type = 0;//ÂàÜËß£
ë„Çø
º„

							if(!FactoringQuaternionXYZ(rotation, xRotation, yRotation,
zRotation))
							{
								if (!FactoringQuaternionYZX(rotation, yRotation, zRotation,
xRotation))
								{
									FactoringQuaternionZXY(rotation, zRotation, xRotation,
yRotation);
									type = 2;
								}
								else
									type = 1;
							}
							else type = 0;

							//cout<<xRotation<<" "<<yRotation<<" "<<zRotation<<endl;
							xRotation = glm::clamp(xRotation, IKLink->lowerLimit.x,
IKLink->upperLimit.x);
							yRotation = glm::clamp(yRotation, IKLink->lowerLimit.y,
IKLink->upperLimit.y);
							zRotation = glm::clamp(zRotation, IKLink->lowerLimit.z,
IKLink->upperLimit.z);

							switch (type)
							{
								case 0:
									rotation = glm::toQuat( glm::eulerAngleX(xRotation) *
glm::eulerAngleY(yRotation) * glm::eulerAngleZ(zRotation) );
//Quaternion.RotationMatrix(Matrix.RotationX(xRotation) *
Matrix.RotationY(yRotation) * Matrix.RotationZ(zRotation));
								break;
								case 1:
									rotation = glm::toQuat( glm::eulerAngleY(yRotation) *
glm::eulerAngleZ(zRotation) * glm::eulerAngleX(xRotation) );
//Quaternion.RotationMatrix(Matrix.RotationY(yRotation) *
Matrix.RotationZ(zRotation) * Matrix.RotationX(xRotation));
								break;
								case 2:
									rotation = glm::toQuat( glm::yawPitchRoll(xRotation,
xRotation, zRotation) ); //Quaternion.RotationYawPitchRoll(xRotation,
xRotation, zRotation);
								break;
							}
						}
						rotMatrix = glm::toMat4(rotation);
						rotation = glm::toQuat(rotMatrix);

						boneRot[IKLink->linkBoneIndex]=rotation *
boneRot[IKLink->linkBoneIndex];
						boneRot[IKLink->linkBoneIndex]=glm::normalize(boneRot[IKLink->linkBone
Index]);

						for( short i = attentionIndex ; i >= 0 ; i-- )
						{
							PMXIKLink *IKLink=IKBone->IKLinks[i];
							PMXBone *bone = pmxInfo.bones[IKLink->linkBoneIndex];
							PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
							bone->Local = glm::translate( bonePos[IKLink->linkBoneIndex] +
bone->position - parent->position ) *
glm::toMat4(boneRot[IKLink->linkBoneIndex]);
							bone->Global = parent->Global * bone->Local;
							skinMatrix[IKLink->linkBoneIndex] = bone->Global *
invBindPose[IKLink->linkBoneIndex];
						}

						PMXBone *bone = pmxInfo.bones[IKBone->IKTargetBoneIndex];
						PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
						bone->Local = glm::translate( bonePos[IKBone->IKTargetBoneIndex]
+ bone->position - parent->position ) *
glm::toMat4(boneRot[IKBone->IKTargetBoneIndex]);
						bone->Global = parent->Global * bone->Local;
						skinMatrix[IKBone->IKTargetBoneIndex] = bone->Global *
invBindPose[IKBone->IKTargetBoneIndex];
					}




				}
			}
		}
	}
}*/

void VMDMotionController::updateIK()
{
	glm::vec4 localEffectorPos(0,0,0,0);
	glm::vec4 localTargetPos(0,0,0,0);

	for(int b=0; b<pmxInfo.bone_continuing_datasets; ++b)
	{
		PMXBone *IKBone=pmxInfo.bones[b];

		if(IKBone->IK) //IK„Éï„É©„Ç
Çí„ÉÅ„
„É
ÇØ
		{
			PMXBone *targetBone=pmxInfo.bones[IKBone->IKTargetBoneIndex];

			for( int iterations = 0 ; iterations < IKBone->IKLoopCount ;
iterations++ )
			{
				for( int attentionIndex = 0 ; attentionIndex < IKBone->IKLinkNum ;
attentionIndex++ )
				{
					PMXIKLink *IKLink=IKBone->IKLinks[attentionIndex];
					PMXBone *linkBone=pmxInfo.bones[IKLink->linkBoneIndex];

					glm::vec4 effectorPos;
					//*****„Åì„Åì„ÅßLocal„Çí‰Ωø„ÅÜ„Å®„ÇÇ„Å£„Å®
Â§â„Å™ÁµêÊûú„ÅåÂá∫„Åü„ÅÆ„ÅßGl
al„Çí‰Ωø„Å£„Å¶„Å
Åô„ÄÇ„ÄÇ„Ä
****
					effectorPos.x = targetBone->Global[3][0];
					effectorPos.y = targetBone->Global[3][1];
					effectorPos.z = targetBone->Global[3][2];
					effectorPos.w = 1;	// ËøΩÂä†Ôºö 4Ê¨°ÂÖ
Éô„ÇØ„Éà„É
Å™„ÅÆ„
Âø
¶Å

					glm::vec4 targetPos;
					targetPos.x = IKBone->Global[3][0];
					targetPos.y = IKBone->Global[3][1];
					targetPos.z = IKBone->Global[3][2];
					targetPos.w = 1;	// ËøΩÂä†Ôºö 4Ê¨°ÂÖ
Éô„ÇØ„Éà„É
Å™„ÅÆ„
Âø
¶Å

					//glm::mat4 invCoord=invBindPose[IKLink->linkBoneIndex];
					glm::mat4 invCoord=glm::inverse(linkBone->Global);

					localEffectorPos = invCoord * effectorPos;
					localTargetPos = invCoord * targetPos;

					glm::vec3
localEffectorDir=glm::normalize(glm::vec3(glm::normalize(localEffectorPos)))
;
					glm::vec3
localTargetDir=glm::normalize(glm::vec3(glm::normalize(localTargetPos)));

					if(linkBone->name.find(u8"„Å≤„Åñ") != string::n
s)
					{
						localEffectorDir = glm::vec3(0, localEffectorDir.y,
localEffectorDir.z);
						localEffectorDir = glm::normalize(localEffectorDir);

						localTargetDir = glm::vec3(0, localTargetDir.y,
localTargetDir.z);
						localTargetDir = glm::normalize(localTargetDir);
					}

					float p = glm::dot(localEffectorDir, localTargetDir);
					if (p > 1 - 1.0e-8f) continue;
					float angle = acos(p);
					//****Ê≥®ÊÑèÔºÅÔºÅÔºÅPMX„ÅÆ„Åì„ÅÆÂ§âÊï∞
„ÅØ„ÇÇ„ÅÜPMD„É¢„Éá„É´„ÅÆÂêå„ÅòÂ§âÊï∞„ÅÆÂõ
Äç„Å´„Å™„Å£„Å¶„ÇãÁÇ∫„ÄÅÂõõ
ç„ÇíÂèñ„ÇãÂøÖË¶Å
Ø„ÅÇ„Çä„Åæ
õ„Çì„Ä
**
					if(angle > IKBone->IKLoopAngleLimit) angle =
IKBone->IKLoopAngleLimit;

					glm::vec3 axis = -glm::cross(localTargetDir, localEffectorDir);//
‰øÆÊ≠£Ôºö Âè≥ÊâãÁ≥ª„
Â∑¶ÊâãÁ≥ª„Åß
ØÂ§ñÁ©ç„
Á¨¶Â
„Å
ÄÜ
					glm::mat4 rotation = glm::rotate(angle, axis);

					if(linkBone->name.find(u8"„Å≤„Åñ") != string::n
s)
					{
						glm::mat4 inv = invBindPose[IKLink->linkBoneIndex];
						glm::mat4 def = inv*linkBone->Global*rotation;
						glm::vec4 t(0,0,1,1);	// ‰øÆÊ≠£Ôº
 = 1
						t= t*def;
						if(t.y < 0) rotation = glm::rotate(-angle, axis);

						float l = glm::length(localTargetPos) /
glm::length(localEffectorPos);
						if(fabs(angle) <= M_PI/2 && l < 1.0f)
						{
							static const float a = 0.5f;
							float diff = acosf(l)*a;

							static const float diff_limit = M_PI/6;

							if(diff > diff_limit)
							{
								diff = diff_limit;
							}
							if(fabs(angle) > 1.0e-6f) diff *= angle/fabs(angle);

							angle += diff;

							glm::vec3 xaxis(1,0,0);
							rotation = glm::rotate(-angle, xaxis);
						}
					}

					boneRot[IKLink->linkBoneIndex]=boneRot[IKLink->linkBoneIndex] *
glm::toQuat(rotation);
					boneRot[IKLink->linkBoneIndex]=glm::normalize(boneRot[IKLink->linkBoneI
ndex]);

					for( short i = attentionIndex ; i >= 0 ; i-- )
					{
						PMXIKLink *IKLink=IKBone->IKLinks[i];
						PMXBone *bone = pmxInfo.bones[IKLink->linkBoneIndex];
						PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
						bone->Local = glm::translate( bonePos[IKLink->linkBoneIndex] +
bone->position - parent->position ) *
glm::toMat4(boneRot[IKLink->linkBoneIndex]);
						bone->Global = parent->Global * bone->Local;
						skinMatrix[IKLink->linkBoneIndex] = bone->Global *
invBindPose[IKLink->linkBoneIndex];
					}

					PMXBone *bone = pmxInfo.bones[IKBone->IKTargetBoneIndex];
					PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
					bone->Local = glm::translate( bonePos[IKBone->IKTargetBoneIndex] +
bone->position - parent->position ) *
glm::toMat4(boneRot[IKBone->IKTargetBoneIndex]);
					bone->Global = parent->Global * bone->Local;
					skinMatrix[IKBone->IKTargetBoneIndex] = bone->Global *
invBindPose[IKBone->IKTargetBoneIndex];

				}
				const float errToleranceSq = 0.000001f;
				if(glm::length2(localEffectorPos - localTargetPos) < errToleranceSq)
				{
					return;
				}
			}
		}
	}
}

void limitAngle(glm::quat &pvec4Out, const glm::quat &pvec4Src, PMXIKLink
*link)
{
	glm::vec3 vec3Angle;

	//glm::eulerAngles(pvec4Src);
	// XYZËª∏Âõû
¢„ÅÆ
ñÂ

	vec3Angle = glm::eulerAngles(pvec4Src) * 3.14159f / 180.f;
//QuaternionToEuler( &vec3Angle, pvec4Src );
	//cout<<vec3Angle.x<<" "<<vec3Angle.y<<" "<<vec3Angle.z<<endl;

	// ËßíÂ
Âà
ôê
	/*if( vec3Angle.x < -3.14159f )	vec3Angle.x = -3.14159f;
	if( -0.002f < vec3Angle.x )		vec3Angle.x = -0.002f;
	if( vec3Angle.y < -3.14159f )	vec3Angle.y = -3.14159f;
	if( -0.002f < vec3Angle.y )		vec3Angle.y = -0.002f;
	if( vec3Angle.z < -3.14159f )	vec3Angle.z = -3.14159f;
	if( -0.002f < vec3Angle.z )		vec3Angle.z = -0.002f;*/

	//cout<<"vec3Angle: "<<vec3Angle.x<<" "<<vec3Angle.y<<"
"<<vec3Angle.z<<endl;
	//cout<<"lowerLimit: "<<link->lowerLimit.x<<" "<<link->lowerLimit.y<<"
"<<link->lowerLimit.z<<endl;
	//cout<<"upperLimit: "<<link->upperLimit.x<<" "<<link->upperLimit.y<<"
"<<link->upperLimit.z<<endl;

	/*if(vec3Angle.x < link->lowerLimit.x * 3.14159f / 180.f ) vec3Angle.x =
link->lowerLimit.x * 3.14159f / 180.f;
	if(vec3Angle.y < link->lowerLimit.y * 3.14159f / 180.f ) vec3Angle.y =
link->lowerLimit.y * 3.14159f / 180.f;
	if(vec3Angle.z < link->lowerLimit.z * 3.14159f / 180.f ) vec3Angle.z =
link->lowerLimit.z * 3.14159f / 180.f;

	if(vec3Angle.x > link->upperLimit.x * 3.14159f / 180.f ) vec3Angle.x =
link->upperLimit.x * 3.14159f / 180.f;
	if(vec3Angle.y > link->upperLimit.y * 3.14159f / 180.f ) vec3Angle.y =
link->upperLimit.y * 3.14159f / 180.f;
	if(vec3Angle.z > link->upperLimit.z * 3.14159f / 180.f ) vec3Angle.z =
link->upperLimit.z * 3.14159f / 180.f;*/

	// XYZËª∏ÂõûËª¢„Åã„
„ÇØ„Ç©„É
Çø„Éã„
„É
Å∏
	//QuaternionCreateEuler( pvec4Out, &vec3Angle );
	glm::quat result(vec3Angle);
	pvec4Out=result;
}

/*void VMDMotionController::updateIK()
{
	//IK Solver utilizing Triangulation
	//http://digital.liby.waikato.ac.nz/conferences/ivcnz07/papers/ivcnz07-pape
r34.pdf

	for(unsigned b=0; b<pmxInfo.bone_continuing_datasets; ++b)
	{
		PMXBone *IKBone=pmxInfo.bones[b];

		if(IKBone->IK)
		{
			PMXBone *targetBone=pmxInfo.bones[IKBone->IKTargetBoneIndex];

			for(int ik=IKBone->IKLinkNum-1; ik>=0; --ik)
			{
				PMXIKLink *IKLink=IKBone->IKLinks[ik];
				PMXBone *linkBone=pmxInfo.bones[IKLink->linkBoneIndex];

				glm::vec3
Ai=glm::normalize(glm::vec3(linkBone->getModelLocalPosition()));
				glm::vec3
Ci=glm::normalize(glm::vec3(linkBone->getModelGlobalPosition() -
IKBone->getModelGlobalPosition()));

				float A; //The length of the joint we are currently moving
				float B; //The length of the remaining chain
				float C; //The distance from the target(IKBone) to the current joint
				A=glm::length(linkBone->getModelLocalPosition());
				B=linkBone->remainingChainLength;
				C=glm::length(glm::vec3(linkBone->getModelGlobalPosition() -
IKBone->getModelGlobalPosition()));

				//cout<<A<<" "<<B<<" "<<C<<endl;
				//cout<<ik<<" "<<linkBone->name<<endl;

				if(C >= A+B)
				{
					//Ai=Ci;
					//linkBone->Local=IKBone->Local;
					glm::mat4 inv=IKBone->Local;
					linkBone->Local[0][0]=inv[0][0];
					linkBone->Local[1][0]=inv[1][0];
					linkBone->Local[2][0]=inv[2][0];

					linkBone->Local[0][1]=inv[0][1];
					linkBone->Local[1][1]=inv[1][1];
					linkBone->Local[2][1]=inv[2][1];

					linkBone->Local[0][2]=inv[0][2];
					linkBone->Local[1][2]=inv[1][2];
					linkBone->Local[2][2]=inv[2][2];
				}
				else if(C < abs(A-B))
				{
					//Ai=-Ci;
					//if(B>A)
					{
						glm::mat4 inv=IKBone->Local;
						linkBone->Local[0][0]=inv[0][0];
						linkBone->Local[1][0]=inv[1][0];
						linkBone->Local[2][0]=inv[2][0];

						linkBone->Local[0][1]=inv[0][1];
						linkBone->Local[1][1]=inv[1][1];
						linkBone->Local[2][1]=inv[2][1];

						linkBone->Local[0][2]=inv[0][2];
						linkBone->Local[1][2]=inv[1][2];
						linkBone->Local[2][2]=inv[2][2];
					}
					else //A>B
					{
						cerr<<"System unsolvable, target cannot be reached"<<endl;
					}
				}
				else
				{
					float cosAngle=glm::dot(Ai,Ci);
					float angle=acos(cosAngle);

					float cosAngle2=(-(B*B - A*A - C*C))/(2*A*C);
					float angle2=acos(cosAngle2);

					float theta=angle - angle2;


					glm::vec3 axis;

					if(Ai==-Ci || Ai==Ci) axis=glm::vec3(0,1,0);
					else axis=glm::cross(Ai,Ci);

					glm::quat rotation=glm::toQuat(glm::rotate(theta,axis));
					//cout<<rotation.x<<" "<<rotation.y<<" "<<rotation.z<<"
"<<rotation.w<<endl;

					int i=IKLink->linkBoneIndex;
					PMXBone *bone = pmxInfo.bones[i];
					PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
					boneRot[i]=rotation * boneRot[i];
					glm::mat4 thisTranslation=glm::translate( bonePos[i] +
bone->position - parent->position );
					bone->Local = thisTranslation * glm::toMat4(boneRot[i]);
				}
				int i=IKLink->linkBoneIndex;
				PMXBone *bone = pmxInfo.bones[i];
				PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
				bone->Global = parent->Global * bone->Local;
				skinMatrix[i] = bone->Global * invBindPose[i];
			}
		}
	}

}*/
/*void VMDMotionController::updateIK()
{
	glm::vec4 linkPos;
	glm::vec4 targetPos;
	for(unsigned b=0; b<pmxInfo.bone_continuing_datasets; ++b)
	{
		PMXBone *IKBone=pmxInfo.bones[b];

		if(IKBone->IK)
		{
			PMXBone *targetBone=pmxInfo.bones[IKBone->IKTargetBoneIndex];

			for(unsigned iterations=0; iterations<IKBone->IKLoopCount;
++iterations)
			{
				for(unsigned ik=0; ik<IKBone->IKLinkNum; ++ik)
				{
					PMXIKLink *IKLink=IKBone->IKLinks[ik];
					PMXBone *linkBone=pmxInfo.bones[IKLink->linkBoneIndex];

					targetPos=targetBone->getModelLocalPosition();
					linkPos=linkBone->getModelLocalPosition();
					glm::vec4 IKPos=IKBone->getModelLocalPosition();

					glm::vec4 targetVector=glm::normalize(targetPos - linkPos);
					glm::vec4 IKVector=glm::normalize(IKPos - linkPos);

					float cosAngle=glm::dot(IKVector,targetVector);
					//if(cosAngle > 1 - 1.0e-5f) continue;
					float angle=acos(cosAngle);
					//if(cosAngle<0) angle= M_PI - angle;
					//if(angle>IKBone->IKLoopAngleLimit) angle =
IKBone->IKLoopAngleLimit;

					glm::vec3
axis=glm::cross(glm::vec3(IKVector),glm::vec3(targetVector));

					glm::quat rotation=glm::toQuat(glm::rotate(angle,axis));
					//cout<<rotation.x<<" "<<rotation.y<<" "<<rotation.z<<"
"<<rotation.w<<endl;

					boneRot[IKLink->linkBoneIndex]=boneRot[IKLink->linkBoneIndex]
*rotation;

					for( short i = ik ; i >= 0 ; i-- )
					{
						PMXIKLink *IKLink=IKBone->IKLinks[i];
						PMXBone *bone = pmxInfo.bones[IKLink->linkBoneIndex];
						PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
						bone->Local = glm::translate( bonePos[IKLink->linkBoneIndex] +
bone->position - parent->position ) *
glm::toMat4(boneRot[IKLink->linkBoneIndex]);
						bone->Global = parent->Global * bone->Local;
						skinMatrix[IKLink->linkBoneIndex] = bone->Global *
invBindPose[IKLink->linkBoneIndex];

						//m_ppBoneList[i]->updateMatrix();
					}
					PMXBone *bone = targetBone;
					PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
					bone->Local = glm::translate( bonePos[b] + bone->position -
parent->position ) * glm::toMat4(boneRot[b]);
					bone->Global = parent->Global * bone->Local;
					skinMatrix[b] = bone->Global * invBindPose[b];
					//m_pEffBone->updateMatrix();
				}
				const float errToleranceSq = 0.000001f;
				if(glm::length2(linkPos - targetPos) < errToleranceSq)
					return;
			}
			//return;
		}
	}
}*/

/*void VMDMotionController::updateIK()
{
	for(unsigned b=0; b<pmxInfo.bone_continuing_datasets; ++b)
	{
		PMXBone *IKBone=pmxInfo.bones[b];

		if(IKBone->IK)
		{
			PMXBone *targetBone=pmxInfo.bones[IKBone->IKTargetBoneIndex];

			for(unsigned iterations=0; iterations<IKBone->IKLoopCount;
++iterations)
			{
				//for(unsigned ik=0; ik<IKBone->IKLinkNum; ++ik)
				{
					unsigned ik=0;

					PMXIKLink *IKLink=IKBone->IKLinks[ik];
					PMXBone *linkBone=pmxInfo.bones[IKLink->linkBoneIndex];

					//cout<<IKBone->name<<" "<<targetBone->name<<"
"<<linkBone->name<<endl;
					//cout<<targetBone->parentBoneIndex<<"
"<<IKLink->linkBoneIndex<<endl;

					//glm::vec4 targetPos=targetBone->getModelLocalPosition();
					//glm::mat4 invCoordMatrix=glm::inverse(linkBone->Global);
					//glm::vec4 targetVector=glm::normalize(targetPos *
invCoordMatrix); //v from linkBone to targetBone

					glm::vec4 targetPos=targetBone->getModelGlobalPosition();
					glm::vec4
parentPos=pmxInfo.bones[targetBone->parentBoneIndex]->getModelGlobalPosition
();
					glm::vec4
IKPos=pmxInfo.bones[IKBone->parentBoneIndex]->getModelGlobalPosition();

					glm::vec4 result=glm::normalize(targetPos - parentPos);
					glm::vec4 result2=glm::normalize(IKPos - parentPos);

					float cosAngle=glm::dot(result,result2);
					float angle=acos(cosAngle);

					glm::vec3 axis=glm::cross(glm::vec3(result),glm::vec3(result2));

					glm::quat rotation=glm::toQuat(glm::rotate(angle,axis));

					glm::quat newRotation=rotation *
boneRot[targetBone->parentBoneIndex];



					int i=targetBone->parentBoneIndex;
					PMXBone *bone = pmxInfo.bones[i];
					PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
					glm::mat4 thisRotation=glm::toMat4(rotation) *
glm::toMat4(boneRot[i]);
					glm::mat4 thisTranslation=glm::translate( bonePos[i] +
bone->position - parent->position );
					bone->Local = thisTranslation * thisRotation;
					bone->Local = glm::translate( bonePos[i] + bone->position -
parent->position ) * glm::toMat4(boneRot[i]);
					bone->Global = parent->Global * bone->Local;
					skinMatrix[i] = bone->Global * invBindPose[i];

					i=IKBone->IKTargetBoneIndex;
					bone = pmxInfo.bones[i];
					parent = pmxInfo.bones[bone->parentBoneIndex];
					thisRotation=glm::toMat4(boneRot[i]);
					thisTranslation=glm::translate( bonePos[i] + bone->position -
parent->position );
					bone->Local = thisTranslation * thisRotation;
					bone->Global = parent->Global * bone->Local;
					skinMatrix[i] = bone->Global * invBindPose[i];
				}
			}

			return;
		}
	}
}*/

/*void VMDMotionController::updateIK()
{
	glm::vec4 localIKPos;
	glm::vec4 localTargetPos;

	// IK Bones
	for(unsigned b=0; b<pmxInfo.bone_continuing_datasets; ++b)
	{
		PMXBone *IKBone=pmxInfo.bones[b];
		if(IKBone->IK)
		{
			PMXBone *targetBone=pmxInfo.bones[IKBone->IKTargetBoneIndex];

			for(unsigned iterations=0; iterations<IKBone->IKLoopCount;
++iterations)
			{
				for(int ik=0; ik<IKBone->IKLinkNum; ++ik)
				{
					//int ik=IKBone->IKLinkNum-1;

					PMXIKLink *IKLink=IKBone->IKLinks[ik];
					PMXBone *linkBone=pmxInfo.bones[IKLink->linkBoneIndex]; //Bone
under the effect of the IK Bone

					glm::vec4 targetPos=targetBone->getModelLocalPosition();
					glm::vec4 IKPos=IKBone->getModelLocalPosition();

					glm::mat4 invCoord=glm::inverse(linkBone->Local);

					localIKPos=invCoord * IKPos;
					localTargetPos=invCoord * targetPos;

					glm::vec3 IKVector=glm::normalize(glm::vec3(localIKPos));
					glm::vec3 targetVector=glm::normalize(glm::vec3(localTargetPos));



					float cosAngle=glm::dot(IKVector,targetVector);
					//if(cosAngle > 1 - 1.0e-5f) continue;
					float angle=acos(cosAngle);
					//cout<<cosAngle<<" "<<angle<<endl;

					glm::vec3 axis=glm::cross(IKVector,targetVector);

					glm::mat4
rotation=glm::rotate((float)angle*180.0f/(float)M_PI,axis);
					//glm::mat4 rotation=glm::toMat4(IKQuat);



					for(int j=ik; j>=0; --j)
					{
						PMXBone *bone=pmxInfo.bones[IKBone->IKLinks[ik]->linkBoneIndex];
//current link bone
						PMXBone *parent=pmxInfo.bones[bone->parentBoneIndex];

						boneRot[IKBone->IKLinks[ik]->linkBoneIndex]=glm::toQuat(rotation)*bone
Rot[IKBone->IKLinks[ik]->linkBoneIndex];

						bone->Local = glm::translate(
bonePos[IKBone->IKLinks[ik]->linkBoneIndex] + bone->position -
parent->position ) *
glm::toMat4(boneRot[IKBone->IKLinks[ik]->linkBoneIndex]);
						bone->Global = parent->Global * bone->Local;
						skinMatrix[IKBone->IKLinks[ik]->linkBoneIndex] = bone->Global *
invBindPose[IKBone->IKLinks[ik]->linkBoneIndex];
					}

				}
				//NOTE: Left out error-tolerance check
				const float errToleranceSq = 0.000001f;
				if(glm::length2(localIKPos - localTargetPos) < errToleranceSq)
				{
					//return;
				}
			}
			return;
		}
	}
}*/

					//cout<<IKBone->name<<" "<<targetBone->name<<"
"<<linkBone->name<<" "<<endl;
					/*cout<<linkBone->Local[0][0]<<" "<<linkBone->Local[1][0]<<"
"<<linkBone->Local[2][0]<<" "<<linkBone->Local[3][0]<<endl
					<<linkBone->Local[0][1]<<" "<<linkBone->Local[1][1]<<"
"<<linkBone->Local[2][1]<<" "<<linkBone->Local[3][1]<<endl
					<<linkBone->Local[0][2]<<" "<<linkBone->Local[1][2]<<"
"<<linkBone->Local[2][2]<<" "<<linkBone->Local[3][2]<<endl
					<<linkBone->Local[0][3]<<" "<<linkBone->Local[1][3]<<"
"<<linkBone->Local[2][3]<<" "<<linkBone->Local[3][3]<<endl<<endl;*/

					/*cout<<"linkname: "<<linkBone->name<<endl;
					//cout<<"invCoord: "<<invCoord.x<<" "<<invCoord.y<<"
"<<invCoord.z<<endl;
					cout<<"IKPos: "<<IKPos.x<<" "<<IKPos.y<<" "<<IKPos.z<<endl;
					cout<<"targetPos: "<<targetPos.x<<" "<<targetPos.y<<"
"<<targetPos.z<<endl;
					cout<<"localIKPos: "<<localIKPos.x<<" "<<localIKPos.y<<"
"<<localIKPos.z<<endl;
					cout<<"localTargetPos: "<<localTargetPos.x<<"
"<<localTargetPos.y<<" "<<localTargetPos.z<<endl;*/

void VMDMotionController::advanceTime() { ++time; }


float ipfunc(float t, float p1, float p2)
{
	return ((1 + 3 * p1 - 3 * p2) * t * t * t + (3 * p2 - 6 * p1) * t * t + 3
* p1 * t);
}

float ipfuncd(float t, float p1, float p2)
{
	return ((3 + 9 * p1 - 9 * p2) * t * t + (6 * p2 - 12 * p1) * t + 3 * p1);
}

float bezierp(float x1, float x2, float y1, float y2, float x)
{
/*interpolate using Bezier curve (http://musashi.or.tv/fontguide_doc3.htm)
Bezier curve is parametrized by t (0 <= t <= 1)
x = s^3 x_0 + 3 s^2 t x_1 + 3 s t^2 x_2 + t^3 x_3
y = s^3 y_0 + 3 s^2 t y_1 + 3 s t^2 y_2 + t^3 y_3
where s is defined as s = 1 - t.
Especially, for MMD, (x_0, y_0) = (0, 0) and (x_3, y_3) = (1, 1), so
x = 3 s^2 t x_1 + 3 s t^2 x_2 + t^3
y = 3 s^2 t y_1 + 3 s t^2 y_2 + t^3
Now, given x, find t by bisection method
(http://en.wikipedia.org/wiki/Bisection_method)
i.e. find t such that f(t) = 3 s^2 t x_1 + 3 s t^2 x_2 + t^3 - x = 0
One thing to note here is that f(t) is monotonically increasing in the range
[0,1]
Therefore, when I calculate f(t) for the t I guessed,
Finally find y for the t.
###
#Adopted from MMDAgent*/
	float &t = x;
	while(true)
	{
		float v = ipfunc(t, x1, x2) - x;
		if(v * v < 0.0000001) break; //Math.abs(v) < 0.0001
		double tt = glm::mix(x1,x2,t);//ipfuncd(t, x1, x2)
		if(tt == 0) break;
		t -= v / tt;
		return ipfunc(t, y1, y2);
	}
}

glm::vec4 Vectorize(const glm::quat theQuat)
{
	glm::vec4 ret;

	ret.x = theQuat.x;
	ret.y = theQuat.y;
	ret.z = theQuat.z;
	ret.w = theQuat.w;

	return ret;
}

glm::fquat Lerp(const glm::quat &v0, const glm::quat &v1, float alpha)
{
    glm::vec4 start = Vectorize(v0);
    glm::vec4 end = Vectorize(v1);
    glm::vec4 interp = glm::mix(start, end, alpha);
    interp = glm::normalize(interp);
    return glm::fquat(interp.w, interp.x, interp.y, interp.z);
}

glm::fquat Slerp(glm::quat &v0, glm::quat &v1, float alpha)
{
	float dot = glm::dot(v0, v1);

	if(dot<0.0f)
	{
		dot=-dot;
		v0=-v0;
	}

	const float DOT_THRESHOLD = 0.9995f;
	if(dot > DOT_THRESHOLD)
		return Lerp(v0, v1, alpha);

	glm::clamp(dot, -1.0f, 1.0f);
	float theta_0 = acosf(dot);
	float theta = theta_0*alpha;

	glm::quat v2 = v1 + -v0*dot;
	v2 = glm::normalize(v2);

    return v0*cos(theta) + v2*sin(theta);
}

glm::vec4 Lerp(const glm::vec4 &start, const glm::vec4 &end, float alpha)
{
    glm::vec4 interp = glm::mix(start, end, alpha);
    interp = glm::normalize(interp);
    return interp;
}

glm::vec4 Slerp(glm::vec4 &v0, glm::vec4 &v1, float alpha)
{
	float dot = glm::dot(v0, v1);

	if(dot<0.0f)
	{
		dot=-dot;
		v0=-v0;
	}

	const float DOT_THRESHOLD = 0.9995f;
	if(dot > DOT_THRESHOLD)
		return Lerp(v0, v1, alpha);

	glm::clamp(dot, -1.0f, 1.0f);
	float theta_0 = acosf(dot);
	float theta = theta_0*alpha;

	glm::vec4 v2 = v1 + -v0*dot;
	v2 = glm::normalize(v2);

    return v0*cos(theta) + v2*sin(theta);
}

float Lerp(const float &start, const float &end, float alpha)
{
    float interp = glm::mix(start, end, alpha);
    interp = glm::normalize(interp);
    return interp;
}

float Slerp(float &v0, float &v1, float alpha)
{
	float dot = glm::dot(v0, v1);

	if(dot<0.0f)
	{
		dot=-dot;
		v0=-v0;
	}

	const float DOT_THRESHOLD = 0.9995f;
	if(dot > DOT_THRESHOLD)
		return Lerp(v0, v1, alpha);

	//glm::clamp(dot, -1.0f, 1.0f);
	float theta_0 = acosf(dot);
	float theta = theta_0*alpha;

	float v2 = v1 + -v0*dot;
	v2 = glm::normalize(v2);

    return v0*cos(theta) + v2*sin(theta);
}

#include "motioncontroller.h"

#include "interpolation.h"

#include <sstream>
#include <iostream>

#define GLM_FORCE_RADIANS

#include <glm/gtc/matrix_transform.hpp> 
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/euler_angles.hpp>

//#define MODELDUMP true

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;

VMDMotionController::VMDMotionController(PMXInfo &pmxInfovar,VMDInfo &vmdInfovar,GLuint shaderProgram):pmxInfo(pmxInfovar),vmdInfo(vmdInfovar)
{	
	//***INIT BONE TRANSFORMATION VARIABLES***
	time=0;
	
	invBindPose=new glm::mat4[pmxInfo.bone_continuing_datasets];
	for(int i=0; i<pmxInfo.bone_continuing_datasets; ++i)
	{
		PMXBone *b = pmxInfo.bones[i];
		invBindPose[i] = glm::translate( -b->position );
	}
	skinMatrix=new glm::mat4[pmxInfo.bone_continuing_datasets]();
	
	Bones_loc=glGetUniformLocation(shaderProgram,"Bones");
	
	
	boneKeyFrames.resize(pmxInfo.bone_continuing_datasets);
	for(unsigned i=0; i<vmdInfo.boneCount; ++i)
	{
		//cout<<"Searching for match in model for "<<vmdInfo.boneFrames[i].name<<"...";
		for(unsigned j=0; j<pmxInfo.bone_continuing_datasets; ++j)
		{
			//Search for the bone number from the bone name
			if(vmdInfo.boneFrames[i].name == pmxInfo.bones[j]->name)
			{
				//cout<<"Match found";
				boneKeyFrames[j].push_back(vmdInfo.boneFrames[i]);
				continue;
			}
			else
			{
				//cout<<vmdInfo.boneFrames[i].name<<" "<<pmxInfo.bones[j]->name<<endl;
			}
		}
		//cout<<endl;
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
	vertexData = (VertexData*)malloc(pmxInfo.vertex_continuing_datasets*sizeof(VertexData)*2);
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
                   
		//cout<<vertexData[i].position.x<<" "<<vertexData[i].position.y<<" "<<vertexData[i].position.z<<" "<<vertexData[i].position.w<<endl;
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
	glBufferData(GL_ARRAY_BUFFER, pmxInfo.vertex_continuing_datasets*sizeof(VertexData), vertexData, GL_DYNAMIC_DRAW);
	
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
		vertexData[i].position.w=1.0f;
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
				t0=(*ite_morphKeyFrames[i]).frame*2; //MMD=30fps, this program=60fps, 60/30=2
				val0= (*ite_morphKeyFrames[i]).value;
				
				//cout<<morph->name<<" "<<(*ite_morphKeyFrames[i]).name<<val0<<endl;
			
				if(++ite_morphKeyFrames[i] != morphKeyFrames[i].end())
				{
					t1 = (*ite_morphKeyFrames[i]).frame*2;
					val1 = (*ite_morphKeyFrames[i]).value;
				
					float s = (float)(time - t0)/(float)(t1 - t0);	//Linear Interpolation
					
					
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
				
				glm::vec3 diffVector=morphTarget;
				
				vertexData[vMorph->vertexIndex].position.x+=diffVector.x * vMorphWeights[i];
				vertexData[vMorph->vertexIndex].position.y+=diffVector.y * vMorphWeights[i];
				vertexData[vMorph->vertexIndex].position.z+=diffVector.z * vMorphWeights[i];
				vertexData[vMorph->vertexIndex].position.w=1.0f;
				//vertexData[vMorph->vertexIndex].position.w=vertexPosition.w;
			}
			
		}
	}
	glBufferData(GL_ARRAY_BUFFER, pmxInfo.vertex_continuing_datasets*sizeof(VertexData), vertexData, GL_DYNAMIC_DRAW);
}

void VMDMotionController::updateBoneMatrix()
{	
	glUniformMatrix4fv(Bones_loc, pmxInfo.bone_continuing_datasets, GL_FALSE, (const GLfloat*)skinMatrix);
}

void VMDMotionController::updateBoneAnimation()
{
	//cout<<"Bezier interpol: "<<bezier(0.25f,0.25f,0.1f,0.25f,1.0f)<<endl;
	//exit(EXIT_FAILURE);
	
	
	
	PMXBone   *b  = pmxInfo.bones[0];
	
	//Root+FKBones
	for(unsigned i = 0; i < pmxInfo.bone_continuing_datasets; i++)
	{
		b  = pmxInfo.bones[i];
		
		unsigned long t0,t1;
		glm::quat q0,q1;
		glm::vec3 p0,p1;
		
		
		//boneRot[i]=glm::quat(1.0f,0.0f,0.0f,0.0f);
		//bonePos[i]=glm::vec3(0.0f,0.0f,0.0f);
				
		if(ite_boneKeyFrames[i] != boneKeyFrames[i].end())
		{						
			t0=(*ite_boneKeyFrames[i]).frame*2; //MMD=30fps, this program=60fps, 60/30=2
			boneRot[i] = q0 = (*ite_boneKeyFrames[i]).rotation;
			bonePos[i] = p0 = (*ite_boneKeyFrames[i]).translation;
			
			BezierParameters &bez=(*ite_boneKeyFrames[i]).bezier;
			
			if(++ite_boneKeyFrames[i] != boneKeyFrames[i].end())
			{
				t1 = (*ite_boneKeyFrames[i]).frame*2; //MMD=30fps, this program=60fps, 60/30=2
				q1 = (*ite_boneKeyFrames[i]).rotation;
				p1 = (*ite_boneKeyFrames[i]).translation;
				
				//float s = (float)(time - t0)/(float)(t1 - t0);	//Linear Interpolation
				//cout<<"Bezier: "<<bezier((float)(time - t0)/(float)(t1 - t0), bez.X1.x,bez.X1.y, bez.X2.x,bez.X2.y)<<endl;
				float T=(float)(time - t0)/(float)(t1 - t0);
				float s=0.0f;
				
				//bonePos[i]=p0 + (p1 - p0)*T;
				
				s=bezier(T, bez.X1.x,bez.X1.y, bez.X2.x,bez.X2.y);
				bonePos[i].x=p0.x+ (p1.x-p0.x)*s;
				
				s=bezier(T, bez.Y1.x,bez.Y1.y, bez.Y2.x,bez.Y2.y);
				bonePos[i].y=p0.y+ (p1.y-p0.y)*s;
				
				s=bezier(T, bez.Z1.x,bez.Z1.y, bez.Z2.x,bez.Z2.y);
				bonePos[i].z=p0.z+ (p1.z-p0.z)*s;
				
				s=bezier(T, bez.R1.x,bez.R1.y, bez.R2.x,bez.R2.y);
				boneRot[i]=Slerp(q0,q1,s);
				
				//boneRot[i]=glm::mix(q0,q1,s);
				
				//boneRot[i]=Slerp(q0,q1,T);
				//bonePos[i]=p0 + (p1 - p0)*T;
				
				if(time!=t1) --ite_boneKeyFrames[i];
			}
		}
		
		if(b->parentBoneIndex!=-1)
		{
			PMXBone *parent = pmxInfo.bones[b->parentBoneIndex];
			b->Local = glm::translate( bonePos[i] + b->position - parent->position ) * glm::toMat4(boneRot[i]);
		}
		else
		{
			b->Local = glm::translate( bonePos[i] + b->position ) * glm::toMat4(boneRot[i]);
		}
	}
	
	updateIK();
	
	
	for(unsigned i = 0; i < pmxInfo.bone_continuing_datasets; i++)
	{
		b  = pmxInfo.bones[i];
		skinMatrix[i] = b->calculateGlobalMatrix() * invBindPose[i];
	}
	
	updateBoneMatrix();
}



void VMDMotionController::updateIK()
{
	glm::vec4 effectorPos;
	glm::vec4 targetPos;
	
	glm::vec4 localEffectorPos(0,0,0,0);
	glm::vec4 localTargetPos(0,0,0,0);
	
	glm::vec3 localEffectorDir;
	glm::vec3 localTargetDir;
	
	glm::mat4 tmpMatrix;
	
	glm::mat4 invCoord;
	
	glm::vec3 axis;
	glm::mat4 rotation;

	for(int b=0; b<pmxInfo.bone_continuing_datasets; ++b)
	{
		PMXBone *IKBone=pmxInfo.bones[b];

		if(IKBone->IK) //IKフラグをチェック
		{
			//cout<<IKBone->name<<endl;
			
			glm::mat4 IKBoneMatrix=IKBone->calculateGlobalMatrix();
			
			PMXBone *targetBone=pmxInfo.bones[IKBone->IKTargetBoneIndex];
			
			//cout<<targetBone->name<<endl;
			
			//cout<<IKBone->IKLoopCount<<endl;
			for( int iterations = 0; iterations < IKBone->IKLoopCount; iterations++ )
			{
				for( int attentionIndex=0 ; attentionIndex<IKBone->IKLinkNum; attentionIndex++ )
				{
					PMXIKLink *IKLink=IKBone->IKLinks[attentionIndex];
					PMXBone *linkBone=pmxInfo.bones[IKLink->linkBoneIndex];
					
					//cout<<linkBone->name<<endl;

					tmpMatrix=targetBone->calculateGlobalMatrix();
					effectorPos.x = tmpMatrix[3][0];
					effectorPos.y = tmpMatrix[3][1];
					effectorPos.z = tmpMatrix[3][2];
					effectorPos.w = tmpMatrix[3][3];

					targetPos.x = IKBoneMatrix[3][0];
					targetPos.y = IKBoneMatrix[3][1];
					targetPos.z = IKBoneMatrix[3][2];
					targetPos.w = IKBoneMatrix[3][3];
					
					invCoord=glm::inverse(linkBone->calculateGlobalMatrix());

					localEffectorPos = invCoord * effectorPos;
					localTargetPos = invCoord * targetPos;

					localEffectorDir=glm::normalize(glm::vec3(glm::normalize(localEffectorPos)));
					localTargetDir=glm::normalize(glm::vec3(glm::normalize(localTargetPos)));

					if(linkBone->name.find(u8"ひざ") != string::npos)
					{
						localEffectorDir = glm::vec3(0, localEffectorDir.y, localEffectorDir.z);
						//localEffectorDir = glm::vec3(0, localEffectorDir.y, localEffectorDir.z);
						localEffectorDir = glm::normalize(localEffectorDir);

						localTargetDir = glm::vec3(0, localTargetDir.y, localTargetDir.z);
						localTargetDir = glm::normalize(localTargetDir);
					}
					
					//cout<<localEffectorDir.x<<" "<<localEffectorDir.y<<" "<<localEffectorDir.z<<endl;
					//cout<<localTargetDir.x<<" "<<localTargetDir.y<<" "<<localTargetDir.z<<endl;

					float p = glm::dot(localEffectorDir, localTargetDir);
					//cout<<"P: "<<p<<endl;
					if (p > 1 - 1.0e-6f) continue;
					float angle = acos(p);
					//****注意！！！PMXのこの変数はもうPMDモデルの同じ変数の四倍になってる為、四倍を取る必要はありません。***
					if(angle > IKBone->IKLoopAngleLimit) angle = IKBone->IKLoopAngleLimit;

					axis = -glm::cross(localTargetDir, localEffectorDir);
					
					/*if(IKLink->angleLimit)
					{
						if(IKLink->lowerLimit.y==0 && IKLink->upperLimit.y==0 && IKLink->lowerLimit.z==0 && IKLink->upperLimit.z==0)
						{
							float vvx=axis.x*linkBone->Local[1][1] + axis.y*linkBone->Local[1][2] + axis.z*linkBone->Local[1][3];
							if(vvx>=0.0f) axis.x=1.0f;
							else		  axis.x=-1.0f;
							axis.y=0.0f;
							axis.z=0.0f;
						}
						else if(IKLink->lowerLimit.x==0 && IKLink->upperLimit.x==0 && IKLink->lowerLimit.z==0 && IKLink->upperLimit.z==0)
						{
							float vvy=axis.x*linkBone->Local[2][1] + axis.y*linkBone->Local[2][2] + axis.z*linkBone->Local[2][3];
							if(vvy>=0.0f) axis.y=1.0f;
							else		  axis.y=-1.0f;
							axis.x=0.0f;
							axis.z=0.0f;
						}
						else if(IKLink->lowerLimit.x==0 && IKLink->upperLimit.x==0 && IKLink->lowerLimit.y==0 && IKLink->upperLimit.y==0)
						{
							float vvz=axis.x*linkBone->Local[3][1] + axis.y*linkBone->Local[3][2] + axis.z*linkBone->Local[3][3];
							if(vvz>=0.0f) axis.z=1.0f;
							else		  axis.z=-1.0f;
							axis.x=0.0f;
							axis.y=0.0f;
						}
					}
					else
					{
						//calculate roll axis
						glm::vec3 vv;
						vv.x=axis.x*linkBone->Local[1][1] + axis.y*linkBone->Local[1][2] + axis.z*linkBone->Local[1][3];
						vv.y=axis.x*linkBone->Local[2][1] + axis.y*linkBone->Local[2][2] + axis.z*linkBone->Local[2][3];
						vv.z=axis.x*linkBone->Local[3][1] + axis.y*linkBone->Local[3][2] + axis.z*linkBone->Local[3][3];
						axis=glm::normalize(vv);
					}*/
					
					rotation = glm::rotate((float)(180.0/M_PI*angle), axis);
					
					/*if(IKLink->angleLimit)
					{						
						// 軸回転角度を算出
						if((IKLink->lowerLimit.x>-1.570796f)&(IKLink->upperLimit.x<1.570796f))
						{
							// Z*X*Y順
							// X軸回り
							float fLimit = 1.535889f;		// 88.0f/180.0f*3.14159265f;
							float fSX = -rotation[3][2];			// sin(θx)
							float fX  = (float)asin(fSX);	// X軸回り決定
							float fCX = (float)cos(fX);

							// ジンバルロック回避
							if(fabs(fX) > fLimit)
							{
								fX = (fX<0)?- fLimit: fLimit;
								fCX = (float)cos(fX);
							}
							
							// Y軸回り
							float fSY = rotation[3][1] / fCX;
							float fCY = rotation[3][3] / fCX;
							float fY = (float)atan2(fSY, fCY);	// Y軸回り決定
							
							// Z軸回り
							float fSZ = rotation[1][2] / fCX;
							float fCZ = rotation[2][2] / fCX;
							float fZ = (float)atan2(fSZ, fCZ);
							
							// 角度の制限
							if(fX < IKLink->lowerLimit.x)
							{
								float tf= 2 * IKLink->lowerLimit.x - fX;
								if(tf <= IKLink->upperLimit.x) fX = tf;
								else						   fX = IKLink->lowerLimit.x;
							}
							if(fX > IKLink->upperLimit.x)
							{
								float tf= 2 * IKLink->upperLimit.x - fX;
								if(tf >= IKLink->lowerLimit.x) fX = tf;
								else						   fX = IKLink->upperLimit.x;
							}
							if(fY < IKLink->lowerLimit.y)
							{
								float tf= 2 * IKLink->lowerLimit.y - fY;
								if(tf <= IKLink->upperLimit.y) fY = tf;
								else						   fY = IKLink->lowerLimit.y;
							}
							if(fY > IKLink->upperLimit.y)
							{
								float tf= 2 * IKLink->upperLimit.y - fY;
								if(tf >= IKLink->lowerLimit.y) fY = tf;
								else						   fY = IKLink->upperLimit.y;
							}
							if(fZ < IKLink->lowerLimit.z)
							{
								float tf= 2 * IKLink->lowerLimit.z - fZ;
								if(tf <= IKLink->upperLimit.z) fZ = tf;
								else						   fZ = IKLink->lowerLimit.z;
							}
							if(fZ > IKLink->upperLimit.z)
							{
								float tf= 2 * IKLink->upperLimit.z - fZ;
								if(tf >= IKLink->lowerLimit.z) fZ = tf;
								else						   fZ = IKLink->upperLimit.z;
							}
							
							glm::mat4 mX,mY,mZ;
							mX=glm::rotate(mX, fX, glm::vec3(1.0f,0.0f,0.0f));
							mY=glm::rotate(mY, fY, glm::vec3(0.0f,1.0f,0.0f));
							mZ=glm::rotate(mZ, fZ, glm::vec3(0.0f,0.0f,1.0f));
							
							//mX=glm::rotate(mX, (float)(fX*180.0f/M_PI), glm::vec3(1.0f,0.0f,0.0f));
							//mY=glm::rotate(mY, (float)(fY*180.0f/M_PI), glm::vec3(0.0f,1.0f,0.0f));
							//mZ=glm::rotate(mZ, (float)(fZ*180.0f/M_PI), glm::vec3(0.0f,0.0f,1.0f));
							
							rotation=mZ * mX * mY;
						}
						else if((IKLink->lowerLimit.y>-1.570796f)&(IKLink->upperLimit.y<1.570796f))
						{
							// X*Y*Z順
							// Y軸回り
							float fLimit = 1.535889f;		// 88.0f/180.0f*3.14159265f;
							float fSY = -rotation[1][3];			// sin(θy)
							float fY  = (float)asin(fSY);	// Y軸回り決定
							float fCY = (float)cos(fY);

							// ジンバルロック回避
							if(fabs(fY) > fLimit){
								fY = (fY<0)?- fLimit: fLimit;
								fCY = (float)cos(fY);
							}

							// X軸回り
							float fSX = rotation[2][3] / fCY;
							float fCX = rotation[3][3] / fCY;
							float fX  = (float)atan2(fSX, fCX);	// X軸回り決定

							// Z軸回り
							float fSZ = rotation[1][2] / fCY;
							float fCZ = rotation[1][1] / fCY;
							float fZ  = (float)atan2(fSZ, fCZ);	// Z軸回り決定

							// 角度の制限
							if(fX < IKLink->lowerLimit.x)
							{
								float tf= 2 * IKLink->lowerLimit.x - fX;
								if(tf <= IKLink->upperLimit.x) fX = tf;
								else						   fX = IKLink->lowerLimit.x;
							}
							if(fX > IKLink->upperLimit.x)
							{
								float tf= 2 * IKLink->upperLimit.x - fX;
								if(tf >= IKLink->lowerLimit.x) fX = tf;
								else						   fX = IKLink->upperLimit.x;
							}
							if(fY < IKLink->lowerLimit.y)
							{
								float tf= 2 * IKLink->lowerLimit.y - fY;
								if(tf <= IKLink->upperLimit.y) fY = tf;
								else						   fY = IKLink->lowerLimit.y;
							}
							if(fY > IKLink->upperLimit.y)
							{
								float tf= 2 * IKLink->upperLimit.y - fY;
								if(tf >= IKLink->lowerLimit.y) fY = tf;
								else						   fY = IKLink->upperLimit.y;
							}
							if(fZ < IKLink->lowerLimit.z)
							{
								float tf= 2 * IKLink->lowerLimit.z - fZ;
								if(tf <= IKLink->upperLimit.z) fZ = tf;
								else						   fZ = IKLink->lowerLimit.z;
							}
							if(fZ > IKLink->upperLimit.z)
							{
								float tf= 2 * IKLink->upperLimit.z - fZ;
								if(tf >= IKLink->lowerLimit.z) fZ = tf;
								else						   fZ = IKLink->upperLimit.z;
							}

							// 決定した角度でベクトルを回転
							glm::mat4 mX,mY,mZ;
							mX=glm::rotate(mX, fX, glm::vec3(1.0f,0.0f,0.0f));
							mY=glm::rotate(mY, fY, glm::vec3(0.0f,1.0f,0.0f));
							mZ=glm::rotate(mZ, fZ, glm::vec3(0.0f,0.0f,1.0f));
							
							//mX=glm::rotate(mX, (float)(fX*180.0f/M_PI), glm::vec3(1.0f,0.0f,0.0f));
							//mY=glm::rotate(mY, (float)(fY*180.0f/M_PI), glm::vec3(0.0f,1.0f,0.0f));
							//mZ=glm::rotate(mZ, (float)(fZ*180.0f/M_PI), glm::vec3(0.0f,0.0f,1.0f));
							
							rotation=mY * mX * mZ;
						}
						else
						{
							// Y*Z*X順
							// Z軸回り
							float fLimit = 1.535889f;		// 88.0f/180.0f*3.14159265f;
							float fSZ = -rotation[2][1];			// sin(θy)
							float fZ  = (float)asin(fSZ);	// Y軸回り決定
							float fCZ = (float)cos(fZ);

							// ジンバルロック回避
							if(fabs(fZ) > fLimit){
								fZ = (fZ<0)?- fLimit: fLimit;
								fCZ = (float)cos(fZ);
							}

							// X軸回り
							float fSX = rotation[2][3] / fCZ;
							float fCX = rotation[2][2] / fCZ;
							float fX  = (float)atan2(fSX, fCX);	// X軸回り決定

							// Y軸回り
							float fSY = rotation[3][1] / fCZ;
							float fCY = rotation[1][1] / fCZ;
							float fY  = (float)atan2(fSY, fCY);	// Z軸回り決定

							// 角度の制限
							if(fX < IKLink->lowerLimit.x){
								float tf= 2 * IKLink->lowerLimit.x - fX;
								if(tf <= IKLink->upperLimit.x) fX = tf;
								else						   fX = IKLink->lowerLimit.x;
							}
							if(fX > IKLink->upperLimit.x){
								float tf= 2 * IKLink->upperLimit.x - fX;
								if(tf >= IKLink->lowerLimit.x) fX = tf;
								else						   fX = IKLink->upperLimit.x;
							}
							if(fY < IKLink->lowerLimit.y){
								float tf= 2 * IKLink->lowerLimit.y - fY;
								if(tf <= IKLink->upperLimit.y) fY = tf;
								else						   fY = IKLink->lowerLimit.y;
							}
							if(fY > IKLink->upperLimit.y){
								float tf= 2 * IKLink->upperLimit.y - fY;
								if(tf >= IKLink->lowerLimit.y) fY = tf;
								else						   fY = IKLink->upperLimit.y;
							}
							if(fZ < IKLink->lowerLimit.z){
								float tf= 2 * IKLink->lowerLimit.z - fZ;
								if(tf <= IKLink->upperLimit.z) fZ = tf;
								else						   fZ = IKLink->lowerLimit.z;
							}
							if(fZ > IKLink->upperLimit.z){
								float tf= 2 * IKLink->upperLimit.z - fZ;
								if(tf >= IKLink->lowerLimit.z) fZ = tf;
								else						   fZ = IKLink->upperLimit.z;
							}
							
							// 決定した角度でベクトルを回転
							glm::mat4 mX,mY,mZ;
							mX=glm::rotate(mX, fX, glm::vec3(1.0f,0.0f,0.0f));
							mY=glm::rotate(mY, fY, glm::vec3(0.0f,1.0f,0.0f));
							mZ=glm::rotate(mZ, fZ, glm::vec3(0.0f,0.0f,1.0f));
							
							//mX=glm::rotate(mX, (float)(fX*180.0f/M_PI), glm::vec3(1.0f,0.0f,0.0f));
							//mY=glm::rotate(mY, (float)(fY*180.0f/M_PI), glm::vec3(0.0f,1.0f,0.0f));
							//mZ=glm::rotate(mZ, (float)(fZ*180.0f/M_PI), glm::vec3(0.0f,0.0f,1.0f));
							
							rotation=mY * mZ *mX;
						}
					}*/
					
					/*if(linkBone->name.find(u8"ひざ") != string::npos)
					{
						glm::quat rot=glm::toQuat(rotation);
						cout<<"rotation: "<<rot.x<<" "<<rot.y<<" "<<rot.z<<" "<<rot.w<<endl;
					}*/

					if(linkBone->name.find(u8"ひざ") != string::npos)
					{
						glm::mat4 inv = invBindPose[IKLink->linkBoneIndex];
						glm::mat4 def = inv*linkBone->calculateGlobalMatrix()*rotation;
						//glm::vec4 t(0,0,0,1); //mhage method was-> glm::vec4 t(0,0,1,1);
						glm::vec4 t(0,0,1,1);

						t= t*def;
						if(t.y < 0) rotation = glm::rotate(-(float)(180.0/M_PI*angle), axis);

						float l = glm::length(localTargetPos) / glm::length(localEffectorPos);
						if(fabs(angle) <= M_PI/2.0 && l < 1.0f)
						{
							static const float a = 0.5f;
							float diff = acos(l)*a;

							static const float diff_limit = M_PI/6;

							if(diff > diff_limit)
							{
								diff = diff_limit;
							}
							if(abs(angle) > 1.0e-6f) diff *= angle/abs(angle);

							angle += diff;

							glm::vec3 xaxis(1,0,0);
							rotation = glm::rotate((float)(180.0/M_PI*angle), xaxis);
						}
					}
					
					/*cout<<"LinkBone Prior Rotation Matrix: "<<endl;
					cout<<linkBone->Local[0][0]<<" "<<linkBone->Local[0][1]<<" "<<linkBone->Local[0][2]<<" "<<linkBone->Local[0][3]<<endl
					<<linkBone->Local[1][1]<<" "<<linkBone->Local[1][1]<<" "<<linkBone->Local[1][2]<<" "<<linkBone->Local[1][3]<<endl
					<<linkBone->Local[2][2]<<" "<<linkBone->Local[2][1]<<" "<<linkBone->Local[2][2]<<" "<<linkBone->Local[2][3]<<endl
					<<linkBone->Local[3][2]<<" "<<linkBone->Local[3][1]<<" "<<linkBone->Local[3][2]<<" "<<linkBone->Local[3][3]<<endl<<endl;*/
					
					linkBone->Local = linkBone->Local * rotation;
					
					/*cout<<"LinkBone Final Rotation Matrix: "<<endl;
					cout<<linkBone->Local[0][0]<<" "<<linkBone->Local[0][1]<<" "<<linkBone->Local[0][2]<<" "<<linkBone->Local[0][3]<<endl
					<<linkBone->Local[1][1]<<" "<<linkBone->Local[1][1]<<" "<<linkBone->Local[1][2]<<" "<<linkBone->Local[1][3]<<endl
					<<linkBone->Local[2][2]<<" "<<linkBone->Local[2][1]<<" "<<linkBone->Local[2][2]<<" "<<linkBone->Local[2][3]<<endl
					<<linkBone->Local[3][2]<<" "<<linkBone->Local[3][1]<<" "<<linkBone->Local[3][2]<<" "<<linkBone->Local[3][3]<<endl<<endl;*/
					
					//skinMatrix[IKLink->linkBoneIndex] = linkBone->calculateGlobalMatrix() * invBindPose[IKLink->linkBoneIndex];

				}
				const float errToleranceSq = 0.00001f;
				if(glm::length2(localEffectorPos - localTargetPos) < errToleranceSq)
				{
					break;
				}
			}
		}
	}
}

/*void VMDMotionController::updateIK()
{
	glm::vec4 localEffectorPos(0,0,0,0);
	glm::vec4 localTargetPos(0,0,0,0);
	
	for(int b=0; b<pmxInfo.bone_continuing_datasets; ++b)
	{
		PMXBone *IKBone=pmxInfo.bones[b];
		
		if(IKBone->IK) //IKフラグをチェック
		{
			PMXBone *targetBone=pmxInfo.bones[IKBone->IKTargetBoneIndex];
				
			for( int iterations = 0 ; iterations < IKBone->IKLoopCount ; iterations++ )
			{
				for( int attentionIndex = 0 ; attentionIndex < IKBone->IKLinkNum ; attentionIndex++ )
				{
					PMXIKLink *IKLink=IKBone->IKLinks[attentionIndex];
					PMXBone *linkBone=pmxInfo.bones[IKLink->linkBoneIndex];
			
					glm::vec4 effectorPos;
					effectorPos.x = targetBone->Global[3][0];
					effectorPos.y = targetBone->Global[3][1];
					effectorPos.z = targetBone->Global[3][2];
					effectorPos.w = targetBone->Global[3][3];
					
					glm::vec4 targetPos;
					targetPos.x = IKBone->Global[3][0];
					targetPos.y = IKBone->Global[3][1];
					targetPos.z = IKBone->Global[3][2];
					targetPos.w = IKBone->Global[3][3];

					//glm::mat4 invCoord=invBindPose[IKLink->linkBoneIndex];
					glm::mat4 invCoord=glm::inverse(linkBone->Global);
					
					localEffectorPos = invCoord * effectorPos;
					localTargetPos = invCoord * targetPos;
					
					glm::vec3 localEffectorDir=glm::normalize(glm::vec3(glm::normalize(localEffectorPos)));
					glm::vec3 localTargetDir=glm::normalize(glm::vec3(glm::normalize(localTargetPos)));
					
					if(linkBone->name.find(u8"ひざ") != string::npos)
					{
						localEffectorDir = glm::vec3(0, localEffectorDir.y, localEffectorDir.z);
						localEffectorDir = glm::normalize(localEffectorDir);
						
						localTargetDir = glm::vec3(0, localTargetDir.y, localTargetDir.z);
						localTargetDir = glm::normalize(localTargetDir);
					}
					
					float p = glm::dot(localEffectorDir, localTargetDir);
					if (p > 1 - 1.0e-8f) continue;
					float angle = acos(p);
					//****注意！！！PMXのこの変数はもうPMDモデルの同じ変数の四倍になってる為、四倍を取る必要はありません。***
					if(angle > IKBone->IKLoopAngleLimit) angle = IKBone->IKLoopAngleLimit;
					
					glm::vec3 axis = glm::cross(localTargetDir, localEffectorDir);
					glm::mat4 rotation = glm::rotate(angle, axis);
					
					if(linkBone->name.find(u8"ひざ") != string::npos)
					{
						glm::mat4 inv = invBindPose[IKLink->linkBoneIndex];
						glm::mat4 def = inv*linkBone->Global*rotation;
						glm::vec4 t(0,0,1, 0);
						t= t*def;
						if(t.y < 0) rotation = glm::rotate(-angle, axis);
						
						float l = glm::length(localTargetPos) / glm::length(localEffectorPos);
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
											
						boneRot[IKLink->linkBoneIndex]=boneRot[IKLink->linkBoneIndex] * glm::toQuat(rotation);
						boneRot[IKLink->linkBoneIndex]=glm::normalize(boneRot[IKLink->linkBoneIndex]);
						
						for( short i = attentionIndex ; i >= 0 ; i-- )
						{
							PMXIKLink *IKLink=IKBone->IKLinks[i];
							PMXBone *bone = pmxInfo.bones[IKLink->linkBoneIndex];
							PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
							bone->Local = glm::translate( bonePos[IKLink->linkBoneIndex] + bone->position - parent->position ) * glm::toMat4(boneRot[IKLink->linkBoneIndex]);
							bone->Global = parent->Global * bone->Local;
							skinMatrix[IKLink->linkBoneIndex] = bone->Global * invBindPose[IKLink->linkBoneIndex];
						}

						PMXBone *bone = pmxInfo.bones[IKBone->IKTargetBoneIndex];
						PMXBone *parent = pmxInfo.bones[bone->parentBoneIndex];
						bone->Local = glm::translate( bonePos[IKBone->IKTargetBoneIndex] + bone->position - parent->position ) * glm::toMat4(boneRot[IKBone->IKTargetBoneIndex]);
						bone->Global = parent->Global * bone->Local;
						skinMatrix[IKBone->IKTargetBoneIndex] = bone->Global * invBindPose[IKBone->IKTargetBoneIndex];
						
				}
				const float errToleranceSq = 0.000001f;
				if(glm::length2(localEffectorPos - localTargetPos) < errToleranceSq)
				{
					return;
				}
			}
		}
	}
}*/

void limitAngle(glm::quat &pvec4Out, const glm::quat &pvec4Src, PMXIKLink *link)
{
	glm::vec3 vec3Angle;

	//glm::eulerAngles(pvec4Src);
	// XYZ軸回転の取得
	vec3Angle = glm::eulerAngles(pvec4Src) * 3.14159f / 180.f; //QuaternionToEuler( &vec3Angle, pvec4Src );
	//cout<<vec3Angle.x<<" "<<vec3Angle.y<<" "<<vec3Angle.z<<endl;

	// 角度制限
	/*if( vec3Angle.x < -3.14159f )	vec3Angle.x = -3.14159f;
	if( -0.002f < vec3Angle.x )		vec3Angle.x = -0.002f;
	if( vec3Angle.y < -3.14159f )	vec3Angle.y = -3.14159f;
	if( -0.002f < vec3Angle.y )		vec3Angle.y = -0.002f;
	if( vec3Angle.z < -3.14159f )	vec3Angle.z = -3.14159f;
	if( -0.002f < vec3Angle.z )		vec3Angle.z = -0.002f;*/
	
	//cout<<"vec3Angle: "<<vec3Angle.x<<" "<<vec3Angle.y<<" "<<vec3Angle.z<<endl;
	//cout<<"lowerLimit: "<<link->lowerLimit.x<<" "<<link->lowerLimit.y<<" "<<link->lowerLimit.z<<endl;
	//cout<<"upperLimit: "<<link->upperLimit.x<<" "<<link->upperLimit.y<<" "<<link->upperLimit.z<<endl;
	
	/*if(vec3Angle.x < link->lowerLimit.x * 3.14159f / 180.f ) vec3Angle.x = link->lowerLimit.x * 3.14159f / 180.f;
	if(vec3Angle.y < link->lowerLimit.y * 3.14159f / 180.f ) vec3Angle.y = link->lowerLimit.y * 3.14159f / 180.f;
	if(vec3Angle.z < link->lowerLimit.z * 3.14159f / 180.f ) vec3Angle.z = link->lowerLimit.z * 3.14159f / 180.f;
	
	if(vec3Angle.x > link->upperLimit.x * 3.14159f / 180.f ) vec3Angle.x = link->upperLimit.x * 3.14159f / 180.f;
	if(vec3Angle.y > link->upperLimit.y * 3.14159f / 180.f ) vec3Angle.y = link->upperLimit.y * 3.14159f / 180.f;
	if(vec3Angle.z > link->upperLimit.z * 3.14159f / 180.f ) vec3Angle.z = link->upperLimit.z * 3.14159f / 180.f;*/

	// XYZ軸回転からクォータニオンへ
	//QuaternionCreateEuler( pvec4Out, &vec3Angle );
	glm::quat result(vec3Angle);
	pvec4Out=result;
}


void VMDMotionController::advanceTime() { ++time; }

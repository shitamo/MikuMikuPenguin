#include "motioncontroller.h"

#include "interpolation.h"
#include "glm_helper.h"

#include <sstream>
#include <iostream>

#define GLM_FORCE_RADIANS

#include "mmp_glm.h"

//#define MODELDUMP true
#define IK_DUMP false

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;
using namespace ClosedMMDFormat;

VMDMotionController::VMDMotionController(PMXInfo &pmxInfo,VMDInfo &vmdInfo,GLuint shaderProgram):pmxInfo(pmxInfo),vmdInfo(vmdInfo)
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
	
	//Bone matrices are passed to the shader through a texture buffer object.
	//A plain "uniform mat4 Bones[N]" array is limited by
	//GL_MAX_VERTEX_UNIFORM_COMPONENTS (often 4096 floats = 256 matrices),
	//and many current models have more bones than that.
	Bones_loc=glGetUniformLocation(shaderProgram,"boneTexture");
	glGenBuffers(1, &boneBuffer);
	glBindBuffer(GL_TEXTURE_BUFFER, boneBuffer);
	glBufferData(GL_TEXTURE_BUFFER, sizeof(glm::mat4)*(pmxInfo.bone_continuing_datasets>0 ? pmxInfo.bone_continuing_datasets : 1), NULL, GL_DYNAMIC_DRAW);
	glGenTextures(1, &boneTexture);
	glActiveTexture(GL_TEXTURE0 + BONE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_BUFFER, boneTexture);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, boneBuffer);
	glActiveTexture(GL_TEXTURE0);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	glUniform1i(Bones_loc, BONE_TEXTURE_UNIT);
	
	
	boneKeyFrames.resize(pmxInfo.bone_continuing_datasets);
	for(unsigned i=0; i<vmdInfo.boneFrames.size(); ++i)
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
		boneRot.push_back(glm::quat(1, 0, 0, 0));
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
		
		//SDEF: pre-compute the per-bone rotation centres (same as MMD/saba)
		{
			PMXVertex *v=pmxInfo.vertices[i];
			glm::vec3 C(0.0f), cr0(0.0f), cr1(0.0f);
			if(v->weight_transform_formula==WEIGHT_FORMULA_SDEF)
			{
				float w0=v->weight1, w1=1.0f-v->weight1;
				glm::vec3 rw = v->R0*w0 + v->R1*w1;
				glm::vec3 r0 = v->C + v->R0 - rw;
				glm::vec3 r1 = v->C + v->R1 - rw;
				C   = v->C;
				cr0 = (v->C + r0)*0.5f;
				cr1 = (v->C + r1)*0.5f;
			}
			vertexData[i].sdefC=C;
			vertexData[i].sdefR0=cr0;
			vertexData[i].sdefR1=cr1;
		}
                   
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
	//Remember which vertex buffer belongs to the model: the caller binds it
	//before constructing us, but other code (e.g. the Bullet debug drawer)
	//binds its own buffers later, so bind it explicitly when uploading.
	{
		GLint bound=0;
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bound);
		vertexBuffer=(GLuint)bound;
	}
	glBufferData(GL_ARRAY_BUFFER, pmxInfo.vertex_continuing_datasets*sizeof(VertexData), vertexData, GL_STREAM_DRAW);
	
	morphKeyFrames.resize(pmxInfo.morph_continuing_datasets);
	for(unsigned i=0; i<vmdInfo.morphFrames.size(); ++i)
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
	delete[] invBindPose;
	delete[] skinMatrix;
	glDeleteTextures(1, &boneTexture);
	glDeleteBuffers(1, &boneBuffer);
	//free(vertexData);
}

void VMDMotionController::updateVertexMorphs()
{
	const int morphCount=pmxInfo.morph_continuing_datasets;
	
	//***1. Keyframe value of every morph (all morph types)***
	for(int i=0; i<morphCount; ++i)
	{
		if(ite_morphKeyFrames[i] != morphKeyFrames[i].end())
		{
			unsigned long t0=(*ite_morphKeyFrames[i]).frame*2; //MMD=30fps, this program=60fps, 60/30=2
			float val0=(*ite_morphKeyFrames[i]).value;
			
			if(++ite_morphKeyFrames[i] != morphKeyFrames[i].end())
			{
				unsigned long t1=(*ite_morphKeyFrames[i]).frame*2;
				float val1=(*ite_morphKeyFrames[i]).value;
				
				float s = (t1>t0) ? ((float)time - (float)t0)/(float)(t1 - t0) : 1.0f;	//Linear Interpolation
				s = glm::clamp(s, 0.0f, 1.0f);
				vMorphWeights[i]=val0 + (val1 - val0)*s;
				
				if(time!=t1) --ite_morphKeyFrames[i];
			}
			else
			{
				vMorphWeights[i]=val0; //past the last key: hold its value
			}
		}
	}
	
	//***2. Effective weights: group morphs distribute their weight to members***
	morphWeights.assign(vMorphWeights.begin(), vMorphWeights.end());
	for(int i=0; i<morphCount; ++i)
	{
		PMXMorph *morph=pmxInfo.morphs[i];
		if(morph->type!=MORPH_TYPE_GROUP || vMorphWeights[i]==0.0f) continue;
		for(int j=0; j<morph->morphOffsetNum; ++j)
		{
			PMXGroupMorph *g=(PMXGroupMorph*)morph->offsetData[j];
			if(g->morphIndex<0 || g->morphIndex>=morphCount || g->morphIndex==i) continue;
			if(pmxInfo.morphs[g->morphIndex]->type==MORPH_TYPE_GROUP) continue; //nesting is not allowed by PMX
			morphWeights[g->morphIndex]+=vMorphWeights[i]*g->morphRate;
		}
	}
	
	//***3. Reset vertices to the rest state***
	for(int i=0; i<pmxInfo.vertex_continuing_datasets; i++)
	{
		vertexData[i].position.x=pmxInfo.vertices[i]->pos.x;
		vertexData[i].position.y=pmxInfo.vertices[i]->pos.y;
		vertexData[i].position.z=pmxInfo.vertices[i]->pos.z;
		vertexData[i].position.w=1.0f;
		vertexData[i].UV.x=pmxInfo.vertices[i]->UV.x;
		vertexData[i].UV.y=pmxInfo.vertices[i]->UV.y;
	}
	
	const int extraUVCount = pmxInfo.extraUVCount>4 ? 4 : (int)pmxInfo.extraUVCount;
	if(extraUVCount>0)
	{
		extraUVs.resize((size_t)pmxInfo.vertex_continuing_datasets*4);
		for(int i=0; i<pmxInfo.vertex_continuing_datasets; i++)
			for(int k=0; k<4; ++k) extraUVs[(size_t)i*4+k]=pmxInfo.vertices[i]->extraUV[k];
	}
	
	//bone morph accumulators
	boneMorphPos.assign(pmxInfo.bone_continuing_datasets, glm::vec3(0.0f));
	boneMorphRot.assign(pmxInfo.bone_continuing_datasets, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
	
	//***4. Vertex and UV morphs***
	for(int i=0; i<morphCount; ++i)
	{
		PMXMorph *morph=pmxInfo.morphs[i];
		float w=morphWeights[i];
		if(w==0.0f) continue;
		
		if(morph->type==MORPH_TYPE_VERTEX)
		{
			for(int j=0; j<morph->morphOffsetNum; ++j)
			{
				PMXVertexMorph *vMorph=(PMXVertexMorph*) morph->offsetData[j];
				if(vMorph->vertexIndex<0 || vMorph->vertexIndex>=pmxInfo.vertex_continuing_datasets) continue;
				glm::vec4 &p=vertexData[vMorph->vertexIndex].position;
				p.x+=vMorph->coordinateOffset.x * w;
				p.y+=vMorph->coordinateOffset.y * w;
				p.z+=vMorph->coordinateOffset.z * w;
			}
		}
		else if(morph->type==MORPH_TYPE_UV)
		{
			for(int j=0; j<morph->morphOffsetNum; ++j)
			{
				PMXUVMorph *uvMorph=(PMXUVMorph*) morph->offsetData[j];
				if(uvMorph->vertexIndex<0 || uvMorph->vertexIndex>=pmxInfo.vertex_continuing_datasets) continue;
				glm::vec2 &uv=vertexData[uvMorph->vertexIndex].UV;
				uv.x+=uvMorph->UVOffsetAmount.x * w;
				uv.y+=uvMorph->UVOffsetAmount.y * w;
			}
		}
		else if(morph->type>=MORPH_TYPE_EXTRA_UV1 && morph->type<=MORPH_TYPE_EXTRA_UV4)
		{
			int k=morph->type-MORPH_TYPE_EXTRA_UV1;
			if(k>=extraUVCount) continue;
			for(int j=0; j<morph->morphOffsetNum; ++j)
			{
				PMXUVMorph *uvMorph=(PMXUVMorph*) morph->offsetData[j];
				if(uvMorph->vertexIndex<0 || uvMorph->vertexIndex>=pmxInfo.vertex_continuing_datasets) continue;
				extraUVs[(size_t)uvMorph->vertexIndex*4+k]+=uvMorph->UVOffsetAmount * w;
			}
		}
		else if(morph->type==MORPH_TYPE_BONE)
		{
			for(int j=0; j<morph->morphOffsetNum; ++j)
			{
				PMXBoneMorph *bMorph=(PMXBoneMorph*) morph->offsetData[j];
				if(bMorph->boneIndex<0 || bMorph->boneIndex>=pmxInfo.bone_continuing_datasets) continue;
				boneMorphPos[bMorph->boneIndex]+=bMorph->inertia * w;
				glm::quat q(bMorph->rotationAmount.w, bMorph->rotationAmount.x, bMorph->rotationAmount.y, bMorph->rotationAmount.z);
				if(glm::length(q)<1.0e-6f) continue;
				q=glm::normalize(q);
				boneMorphRot[bMorph->boneIndex]=glm::normalize(boneMorphRot[bMorph->boneIndex] * glm::slerp(glm::quat(1.0f,0.0f,0.0f,0.0f), q, w));
			}
		}
	}
	
	//***5. Material morphs***
	updateMaterialMorphs();
	
	GLint previous=0;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, pmxInfo.vertex_continuing_datasets*sizeof(VertexData), vertexData, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previous);
}

void VMDMotionController::updateMaterialMorphs()
{
	const int matCount=pmxInfo.material_continuing_datasets;
	
	//multiplicative and additive accumulators per material (MMD: base*mul + add)
	struct Acc { MaterialState mul, add; };
	std::vector<Acc> acc(matCount);
	for(int m=0; m<matCount; ++m)
	{
		MaterialState &mu=acc[m].mul, &ad=acc[m].add;
		mu.diffuse=glm::vec4(1.0f); mu.specular=glm::vec3(1.0f); mu.shininess=1.0f; mu.ambient=glm::vec3(1.0f);
		mu.edgeColor=glm::vec4(1.0f); mu.edgeSize=1.0f; mu.textureMul=glm::vec4(1.0f); mu.textureAdd=glm::vec4(0.0f);
		mu.sphereMul=glm::vec4(1.0f); mu.sphereAdd=glm::vec4(0.0f); mu.toonMul=glm::vec4(1.0f); mu.toonAdd=glm::vec4(0.0f);
		ad.diffuse=glm::vec4(0.0f); ad.specular=glm::vec3(0.0f); ad.shininess=0.0f; ad.ambient=glm::vec3(0.0f);
		ad.edgeColor=glm::vec4(0.0f); ad.edgeSize=0.0f; ad.textureMul=glm::vec4(0.0f); ad.textureAdd=glm::vec4(0.0f);
		ad.sphereMul=glm::vec4(0.0f); ad.sphereAdd=glm::vec4(0.0f); ad.toonMul=glm::vec4(0.0f); ad.toonAdd=glm::vec4(0.0f);
	}
	
	for(int i=0; i<pmxInfo.morph_continuing_datasets; ++i)
	{
		PMXMorph *morph=pmxInfo.morphs[i];
		float w=morphWeights[i];
		if(morph->type!=MORPH_TYPE_MATERIAL || w==0.0f) continue;
		
		for(int j=0; j<morph->morphOffsetNum; ++j)
		{
			PMXMaterialMorph *mm=(PMXMaterialMorph*) morph->offsetData[j];
			int first=mm->materialIndex, last=mm->materialIndex;
			if(mm->materialIndex==-1) { first=0; last=matCount-1; } //all materials
			if(first<0 || last>=matCount) continue;
			
			for(int m=first; m<=last; ++m)
			{
				if(mm->offsetCalculationFormula==0) //multiplication: lerp(1, value, w)
				{
					MaterialState &mu=acc[m].mul;
					mu.diffuse   *= glm::mix(glm::vec4(1.0f), mm->diffuse, w);
					mu.specular  *= glm::mix(glm::vec3(1.0f), mm->specular, w);
					mu.shininess *= glm::mix(1.0f, mm->shininess, w);
					mu.ambient   *= glm::mix(glm::vec3(1.0f), mm->ambient, w);
					mu.edgeColor *= glm::mix(glm::vec4(1.0f), mm->edgeColor, w);
					mu.edgeSize  *= glm::mix(1.0f, mm->edgeSize, w);
					mu.textureMul*= glm::mix(glm::vec4(1.0f), mm->textureCoefficient, w);
					mu.sphereMul *= glm::mix(glm::vec4(1.0f), mm->sphereTextureCoefficient, w);
					mu.toonMul   *= glm::mix(glm::vec4(1.0f), mm->toonTextureCoefficient, w);
				}
				else //addition
				{
					MaterialState &ad=acc[m].add;
					ad.diffuse   += mm->diffuse * w;
					ad.specular  += mm->specular * w;
					ad.shininess += mm->shininess * w;
					ad.ambient   += mm->ambient * w;
					ad.edgeColor += mm->edgeColor * w;
					ad.edgeSize  += mm->edgeSize * w;
					ad.textureAdd+= mm->textureCoefficient * w;
					ad.sphereAdd += mm->sphereTextureCoefficient * w;
					ad.toonAdd   += mm->toonTextureCoefficient * w;
				}
			}
		}
	}
	
	materialStates.resize(matCount);
	for(int m=0; m<matCount; ++m)
	{
		PMXMaterial *base=pmxInfo.materials[m];
		MaterialState &st=materialStates[m];
		const MaterialState &mu=acc[m].mul, &ad=acc[m].add;
		st.diffuse   = base->diffuse   * mu.diffuse   + ad.diffuse;
		st.specular  = base->specular  * mu.specular  + ad.specular;
		st.shininess = base->shininess * mu.shininess + ad.shininess;
		st.ambient   = base->ambient   * mu.ambient   + ad.ambient;
		st.edgeColor = base->edgeColor * mu.edgeColor + ad.edgeColor;
		st.edgeSize  = base->edgeSize  * mu.edgeSize  + ad.edgeSize;
		st.textureMul= mu.textureMul;
		st.textureAdd= ad.textureAdd;
		st.sphereMul = mu.sphereMul;
		st.sphereAdd = ad.sphereAdd;
		st.toonMul   = mu.toonMul;
		st.toonAdd   = ad.toonAdd;
	}
}

void VMDMotionController::updateBoneMatrix()
{
	for(unsigned i = 0; i < pmxInfo.bone_continuing_datasets; i++)
	{
		skinMatrix[i] = pmxInfo.bones[i]->calculateGlobalMatrix() * invBindPose[i];
	}
	glBindBuffer(GL_TEXTURE_BUFFER, boneBuffer);
	glBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(glm::mat4)*pmxInfo.bone_continuing_datasets, skinMatrix);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	
	glActiveTexture(GL_TEXTURE0 + BONE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_BUFFER, boneTexture);
	glActiveTexture(GL_TEXTURE0);
}

//Bone pose from the VMD plus bone morphs
glm::vec3 VMDMotionController::animatedPosition(int i) const
{
	glm::vec3 p=bonePos[i];
	if(i<(int)boneMorphPos.size()) p+=boneMorphPos[i];
	return p;
}

glm::quat VMDMotionController::animatedRotation(int i) const
{
	glm::quat r=boneRot[i];
	if(glm::length(r)<1.0e-6f) r=glm::quat(1.0f,0.0f,0.0f,0.0f);
	if(i<(int)boneMorphRot.size()) r=r*boneMorphRot[i];
	return r;
}

void VMDMotionController::updateBoneAnimation()
{
	//cout<<"Bezier interpol: "<<bezier(0.25f,0.25f,0.1f,0.25f,1.0f)<<endl;
	//exit(EXIT_FAILURE);
	
	//Root+FKBones
	for(unsigned i = 0; i < pmxInfo.bone_continuing_datasets; i++)
	{
		PMXBone   *b  = pmxInfo.bones[i];
		
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
			
			if(++ite_boneKeyFrames[i] != boneKeyFrames[i].end())
			{
				//The interpolation curve stored in a VMD key describes the
				//transition INTO that key, so use the destination key's curve.
				const BezierParameters &bez=(*ite_boneKeyFrames[i]).bezier;
				t1 = (*ite_boneKeyFrames[i]).frame*2; //MMD=30fps, this program=60fps, 60/30=2
				q1 = (*ite_boneKeyFrames[i]).rotation;
				p1 = (*ite_boneKeyFrames[i]).translation;
				
				//float s = (float)(time - t0)/(float)(t1 - t0);	//Linear Interpolation
				//cout<<"Bezier: "<<bezier((float)(time - t0)/(float)(t1 - t0), bez.X1.x,bez.X1.y, bez.X2.x,bez.X2.y)<<endl;
				//guard against duplicate keyframes (t1==t0) and time before the first key
				float T = (t1>t0) ? ((float)time - (float)t0)/(float)(t1 - t0) : 1.0f;
				T = glm::clamp(T, 0.0f, 1.0f);
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
		
		//Rebuild every bone's local transform from its animation pose each
		//frame (bones without keyframes go back to the rest pose).  IK and
		//inheritance then start from a clean state, as in MMD.  Previously
		//bones without keyframes (IK links such as 腕W/ひじW/ひざ) kept the
		//previous frame's IK result, so their twist drifted over time and
		//the skin weighted to them collapsed (arms looked thin).
		glm::vec3 pos=animatedPosition(i);
		glm::quat rot=animatedRotation(i);
		if(b->parent)
		{
			b->Local = glm::translate( pos + b->position - b->parent->position ) * glm::toMat4(rot);
		}
		else
		{
			b->Local = glm::translate( pos + b->position ) * glm::toMat4(rot);
		}
	}

	updateIK();
	updateInheritance();
}

//Raise a rotation to a (possibly negative or fractional) power:
//rate 1 = same rotation, -1 = inverse, 0.5 = half way.
static glm::quat scaleRotation(const glm::quat &q, float rate)
{
	glm::quat n=glm::normalize(q);
	if(n.w<0.0f) n=-n; //shortest path
	float w=glm::clamp(n.w, -1.0f, 1.0f);
	float angle=2.0f*acosf(w);
	float s=sqrtf(glm::max(0.0f, 1.0f-w*w));
	if(s<1.0e-6f || angle<1.0e-6f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 axis(n.x/s, n.y/s, n.z/s);
	return glm::angleAxis(angle*rate, axis);
}

//Local rotation part of a bone's Local matrix (animation + IK [+ inheritance])
static glm::quat localRotation(const PMXBone *b)
{
	return glm::normalize(glm::quat_cast(glm::mat3(b->Local)));
}

//Animated translation of a bone (Local translation minus its rest offset)
static glm::vec3 localTranslation(const PMXBone *b)
{
	glm::vec3 rest = b->parent ? (b->position - b->parent->position) : b->position;
	return glm::vec3(b->Local[3]) - rest;
}

void VMDMotionController::updateInheritance()
{
	//0 = not processed, 1 = in progress (cycle guard), 2 = done
	std::vector<char> state(pmxInfo.bone_continuing_datasets, 0);
	for(int i=0; i<pmxInfo.bone_continuing_datasets; ++i)
	{
		applyInheritance(i, state);
	}
}

void VMDMotionController::applyInheritance(int i, std::vector<char> &state)
{
	if(state[i]!=0) return;
	state[i]=1;
	
	PMXBone *b=pmxInfo.bones[i];
	if((b->giveRotation || b->giveTranslation) &&
	   b->givenParentBoneIndex>=0 && b->givenParentBoneIndex<pmxInfo.bone_continuing_datasets &&
	   b->givenParentBoneIndex!=i)
	{
		//the source bone must be final (it may itself inherit from another bone)
		applyInheritance(b->givenParentBoneIndex, state);
		PMXBone *src=pmxInfo.bones[b->givenParentBoneIndex];
		
		//This bone's own animated pose (keyframes; bones without keys stay at rest)
		glm::quat ownRot=animatedRotation(i);
		glm::vec3 ownPos=animatedPosition(i);
		if(glm::length(ownRot)<1.0e-6f) ownRot=glm::quat(1.0f,0.0f,0.0f,0.0f);
		
		glm::quat rot=ownRot;
		glm::vec3 pos=ownPos;
		if(b->giveRotation)
		{
			rot = ownRot * scaleRotation(localRotation(src), b->giveRate);
		}
		if(b->giveTranslation)
		{
			pos += localTranslation(src) * b->giveRate;
		}
		
		glm::vec3 rest = b->parent ? (b->position - b->parent->position) : b->position;
		b->Local = glm::translate(pos + rest) * glm::toMat4(rot);
	}
	
	state[i]=2;
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

					//cout<<localEffectorDir.x<<" "<<localEffectorDir.y<<" "<<localEffectorDir.z<<endl;
					//cout<<localTargetDir.x<<" "<<localTargetDir.y<<" "<<localTargetDir.z<<endl;

					float p = glm::dot(localEffectorDir, localTargetDir);
					//cout<<"P: "<<p<<endl;
					if (p > 1 - 1.0e-6f) continue;
					float angle = acos(p);
					//****注意！！！PMXのこの変数はもうPMDモデルの同じ変数の四倍になってる為、四倍を取る必要はありません。***
					if(angle > IKBone->IKLoopAngleLimit) angle = IKBone->IKLoopAngleLimit;

					axis = -glm::cross(localTargetDir, localEffectorDir);
					rotation = glm::rotate((float)angle, axis);

					if (IKLink->angleLimit)
					{
						const glm::quat desired_rotation(linkBone->Local * rotation);
						const glm::vec3 desired_euler = glm::eulerAngles(desired_rotation); //radians in GLM >= 0.9.6
						const glm::vec3 clamped_euler = glm::clamp(desired_euler, IKLink->lowerLimit, IKLink->upperLimit);
						const glm::quat clamped_rotation(clamped_euler);
						const glm::mat4 translation = glm::translate(glm::vec3(linkBone->Local[3][0], linkBone->Local[3][1], linkBone->Local[3][2]));
						linkBone->Local = translation * glm::toMat4(clamped_rotation);
					}
					else
					{
						linkBone->Local *= rotation;
					}
				}
				const float errToleranceSq = 0.00001f;
				if(glm::length2(localEffectorPos - localTargetPos) < errToleranceSq)
				{
					break;
				}
			}
		}
	}

	// dump rotation of IK link bone from parent
	if (IK_DUMP)
	{
		for(int b=0; b<pmxInfo.bone_continuing_datasets; ++b)
		{
			const PMXBone &bone=*pmxInfo.bones[b];
			if(!bone.IK) continue;
			for(int attentionIndex=0 ; attentionIndex<bone.IKLinkNum; attentionIndex++)
			{
				const PMXBone &linkBone=*pmxInfo.bones[bone.IKLinks[attentionIndex]->linkBoneIndex];
				if (time == 0)
				{
					cerr<<linkBone.name<<"[X], "<<linkBone.name<<"[Y], "<<linkBone.name<<"[Z]"<<", ";
				}
				else
				{
					const glm::quat q(linkBone.Local);
					const glm::vec3 euler = glm::degrees(toEulerAnglesRadians(q));
					cerr<<euler<<", ";
				}
			}
		}
		cerr<<endl;
	}
}

bool VMDMotionController::advanceTime()
{
	++time;

	// if all boneKeyFrames are finished, return true
	for(unsigned i = 0; i < pmxInfo.bone_continuing_datasets; i++)
	{
		if (ite_boneKeyFrames[i] != boneKeyFrames[i].end())
		{
			return false;
		}
	}
	return true;
}

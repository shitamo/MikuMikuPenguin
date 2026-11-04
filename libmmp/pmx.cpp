#include "pmx.h"
#include <stdexcept>
#include "glm_helper.h"
#include "binreader.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include <fstream>
#include <iostream>
#include <bitset>
#include <sstream>

#include <string.h>
#include <vector>
#include <float.h>


using namespace std;


	std::string UTF16to8(const unsigned short *in)
	{
		std::string out;
		unsigned int codepoint = 0;
		for (in;  *in != 0;  ++in)
		{
			if (*in >= 0xd800 && *in <= 0xdbff)
				codepoint = ((*in - 0xd800) << 10) + 0x10000;
			else
			{
				if (*in >= 0xdc00 && *in <= 0xdfff)
					codepoint |= *in - 0xdc00;
				else
					codepoint = *in;

				if (codepoint <= 0x7f) {
					out.append(1, static_cast<char>(codepoint));
				} else if (codepoint <= 0x7ff) {
					out.append(1, static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f)));
					out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
				} else if (codepoint <= 0xffff) {
					out.append(1, static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f)));
					out.append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
					out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
				} else {
					out.append(1, static_cast<char>(0xf0 | ((codepoint >> 18) & 0x07)));
					out.append(1, static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
					out.append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
					out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
				}
				codepoint = 0;
			}
		}
		return out;
	}
    
namespace ClosedMMDFormat
{

	namespace
	{
		//Text of n bytes (UTF-16LE or UTF-8) to UTF-8; stops at a NUL
		string decodePMXText(const char *bytes, size_t n, bool utf16)
		{
			if(utf16)
			{
				//WARNING: UTF-16 text-pulling code does NOT support the extra (multi-byte) codesets of UTF-16!!!!
				//(an odd last byte is ignored: it used to overwrite the terminator)
				std::vector<unsigned short> c16(n/2+1, 0);
				memcpy(c16.data(), bytes, (n/2)*2);
				return UTF16to8(c16.data());
			}
			return string(bytes, strnlen(bytes, n));
		}

		string readPMXText(BinaryReader &r, const PMXInfo &pmxInfo)
		{
			uint32_t size=r.u32();
			if(size>r.remaining()) r.fail("text length "+std::to_string(size));
			if(!size) return string();
			std::vector<char> buf(size);
			r.raw(buf.data(), size);
			return decodePMXText(buf.data(), size, pmxInfo.unicode_type==PMX_ENCODE_UTF16);
		}

		glm::vec3 readVec3(BinaryReader &r) { glm::vec3 v; v.x=r.f32(); v.y=r.f32(); v.z=r.f32(); return v; }
		glm::vec4 readVec4(BinaryReader &r) { glm::vec4 v; v.x=r.f32(); v.y=r.f32(); v.z=r.f32(); v.w=r.f32(); return v; }
		//Direct3D (left-handed) -> OpenGL position
		glm::vec3 readPos(BinaryReader &r) { glm::vec3 v=readVec3(r); v.z=-v.z; return v; }

		bool validIndexSize(uint8_t s) { return s==1 || s==2 || s==4; }

		float finiteOr0(float v) { return std::isfinite(v) ? v : 0.0f; }
		glm::vec3 finiteOr0(glm::vec3 v) { return glm::vec3(finiteOr0(v.x), finiteOr0(v.y), finiteOr0(v.z)); }

		void deleteMorphData(PMXMorph *morph, PMXMorphData *data)
		{
			switch(morph->type)
			{
				case MORPH_TYPE_GROUP:    delete static_cast<PMXGroupMorph*>(data); break;
				case MORPH_TYPE_VERTEX:   delete static_cast<PMXVertexMorph*>(data); break;
				case MORPH_TYPE_BONE:     delete static_cast<PMXBoneMorph*>(data); break;
				case MORPH_TYPE_MATERIAL: delete static_cast<PMXMaterialMorph*>(data); break;
				default:                  delete static_cast<PMXUVMorph*>(data); break; //UV, extra UV1-4
			}
		}

		//Remove (and delete) the elements of v for which bad(element) is true
		template<class T, class Pred, class Del>
		size_t removeInvalid(std::vector<T*> &v, Pred bad, Del del)
		{
			size_t removed=0;
			auto out=v.begin();
			for(auto it=v.begin(); it!=v.end(); ++it)
			{
				if(bad(*it)) { del(*it); ++removed; }
				else *out++=*it;
			}
			v.erase(out, v.end());
			return removed;
		}
		template<class T, class Pred>
		size_t removeInvalid(std::vector<T*> &v, Pred bad)
		{
			return removeInvalid(v, bad, [](T *p){ delete p; });
		}
	}

	PMXInfo::~PMXInfo()
	{
		for(PMXVertex *v : vertices) delete v;
		for(PMXFace *f : faces) delete f;
		delete[] texturePaths;
		for(PMXMaterial *m : materials) delete m;
		for(PMXBone *b : bones)
		{
			for(PMXIKLink *l : b->IKLinks) delete l;
			delete b;
		}
		for(PMXMorph *m : morphs)
		{
			for(PMXMorphData *d : m->offsetData) deleteMorphData(m, d);
			delete m;
		}
		for(PMXDisplayFrame *f : displayFrames)
		{
			for(PMXDisplayFrameElement *e : f->elements) delete e;
			delete f;
		}
		for(PMXRigidBody *rb : rigidBodies) delete rb;
		for(PMXJoint *j : joints) delete j;
	}

	void getPMXText(ifstream &miku, PMXInfo &pmxInfo, string &result, bool debug)
	{
		uint32_t text_size=0;
		miku.read((char*)&text_size, 4);
		if(!miku || !text_size) return;

		//do not trust the size: it must fit in the rest of the file
		std::streampos here=miku.tellg();
		miku.seekg(0, ios::end);
		std::streampos end=miku.tellg();
		miku.seekg(here);
		if(here<0 || end<here || (uint64_t)(end-here)<text_size)
		{
			miku.setstate(ios::failbit);
			return;
		}
		std::vector<char> buf(text_size);
		miku.read(buf.data(), text_size);
		result=decodePMXText(buf.data(), text_size, pmxInfo.unicode_type==PMX_ENCODE_UTF16);
	}

	void printDebugInfo(PMXInfo &pmxInfo);

	//Resolve bone parent pointers, break parent cycles and set the rest-pose
	//Local matrices (shared by the PMX and PMD loaders).
	void finalizePMXBones(PMXInfo &pmxInfo)
	{
		const int boneCount=pmxInfo.bone_continuing_datasets;
		//resolve parent pointers (parents may have a larger index than the child)
		for(int i = 0; i < boneCount; ++i)
		{
			PMXBone *bone = pmxInfo.bones[i];
			int p = bone->parentBoneIndex;
			if(p >= 0 && p < boneCount && p != i)
			{
				bone->parent = pmxInfo.bones[p];
			}
			else
			{
				if(p != -1) cerr<<"WARNING: bone "<<bone->name<<" has an invalid parent index "<<p<<endl;
				bone->parentBoneIndex = -1;
				bone->parent = NULL;
			}
		}

		//guard against parent cycles, which would make calculateGlobalMatrix()
		//loop forever.  Linear time: walk up from every bone that has not been
		//visited, marking the path; meeting the current path again is a cycle.
		{
			std::vector<int> mark(boneCount, -1); //walk that first reached the bone
			std::vector<char> done(boneCount, 0);
			for(int start = 0; start < boneCount; ++start)
			{
				int i = start;
				while(i >= 0 && !done[i] && mark[i] != start)
				{
					mark[i] = start;
					int p = pmxInfo.bones[i]->parentBoneIndex;
					if(p >= 0 && mark[p] == start && !done[p])
					{
						cerr<<"WARNING: bone parent cycle at "<<pmxInfo.bones[i]->name<<"; detaching it"<<endl;
						pmxInfo.bones[i]->parent = NULL;
						pmxInfo.bones[i]->parentBoneIndex = -1;
						break;
					}
					i = p;
				}
				for(i = start; i >= 0 && !done[i]; i = pmxInfo.bones[i]->parentBoneIndex) done[i] = 1;
			}
		}

		// calculate and set transformation matrix from parent bone
		for(int i = 0; i < boneCount; ++i)
		{
			PMXBone *bone = pmxInfo.bones[i];
			if(bone->parent)
			{
				bone->Local = glm::translate(bone->position - bone->parent->position);
			}
			else
			{
				bone->Local = glm::translate(bone->position);
			}
		}
	}

	//Check every index stored in a loaded model and repair what can be
	//repaired (an invalid optional reference becomes "none"; an entry that
	//cannot work, such as a joint without its rigid body, is dropped).  Run
	//by the PMX and PMD loaders, so that the rest of libmmp and the viewer
	//can use the indices without checking them.
	void sanitizePMXInfo(PMXInfo &m)
	{
		size_t fixes=0;
		auto note=[&](bool bad) { if(bad) ++fixes; return bad; };

		m.vertex_continuing_datasets=m.vertices.size();
		m.face_continuing_datasets=m.faces.size();
		m.material_continuing_datasets=m.materials.size();
		m.bone_continuing_datasets=m.bones.size();
		m.morph_continuing_datasets=m.morphs.size();
		m.rigid_body_continuing_datasets=m.rigidBodies.size();
		if(m.texture_continuing_datasets<0 || !m.texturePaths) m.texture_continuing_datasets=0;
		if(m.extraUVCount>4) m.extraUVCount=4;

		const int vertexCount=m.vertex_continuing_datasets, boneCount=m.bone_continuing_datasets;
		const int textureCount=m.texture_continuing_datasets, materialCount=m.material_continuing_datasets;
		const int morphCount=m.morph_continuing_datasets;
		auto validBone=[&](int b) { return b>=0 && b<boneCount; };

		//vertices: bone indices (the shader would read outside the bone texture)
		for(PMXVertex *v : m.vertices)
		{
			for(int *b : {&v->boneIndex1, &v->boneIndex2, &v->boneIndex3, &v->boneIndex4})
			{
				if(!validBone(*b)) { note(*b!=-1 && *b!=0); *b=0; } //0: unused slot of a model without bones
			}
			if(v->weight_transform_formula>WEIGHT_FORMULA_SDEF) { note(true); v->weight_transform_formula=WEIGHT_FORMULA_BDEF1; }
		}

		//faces: vertex indices (without vertices no face can be kept)
		if(vertexCount==0) fixes+=removeInvalid(m.faces, [](PMXFace*){ return true; });
		m.face_continuing_datasets=m.faces.size();
		for(PMXFace *f : m.faces)
		{
			for(int k=0; k<3; ++k)
			{
				if(f->points[k]>=(unsigned)vertexCount) { note(true); f->points[k]=0; }
			}
		}

		//materials: texture indices and face ranges (the draw calls must stay
		//inside the index buffer; a count that is not a multiple of 3 is kept,
		//as the later materials start after it and GL ignores the remainder)
		long long indicesLeft=(long long)m.faces.size()*3;
		for(PMXMaterial *mat : m.materials)
		{
			for(int *t : {&mat->textureIndex, &mat->sphereIndex, &mat->toonTextureIndex})
			{
				if(*t<-1 || *t>=textureCount) { note(true); *t=-1; }
			}
			if(mat->shareToon>1) { note(true); mat->shareToon=0; }
			if(mat->shareToon==1 && mat->shareToonTexture>9) { note(true); mat->shareToonTexture=0; }
			if(mat->sphereMode>3) { note(true); mat->sphereMode=0; }
			long long n=mat->hasFaceNum;
			if(n<0 || n>indicesLeft)
			{
				note(true);
				n=(n<0) ? 0 : indicesLeft;
				mat->hasFaceNum=(int)n;
			}
			indicesLeft-=n;
		}

		//bones: references to other bones; the IK loop count is capped like MMD
		for(int i=0; i<boneCount; ++i)
		{
			PMXBone *b=m.bones[i];
			if(b->connectionDisplayMethod && !validBone(b->connectionBoneIndex))
			{
				note(b->connectionBoneIndex!=-1);
				b->connectionBoneIndex=-1;
			}
			if((b->giveRotation || b->giveTranslation) && (!validBone(b->givenParentBoneIndex) || b->givenParentBoneIndex==i))
			{
				note(true);
				b->giveRotation=b->giveTranslation=false;
				b->givenParentBoneIndex=-1;
			}
			if(b->IK)
			{
				if(!validBone(b->IKTargetBoneIndex)) { note(true); b->IK=false; }
				fixes+=removeInvalid(b->IKLinks, [&](PMXIKLink *l){ return !validBone(l->linkBoneIndex); });
				if(b->IKLoopCount>255) b->IKLoopCount=255;
				if(!std::isfinite(b->IKLoopAngleLimit)) { note(true); b->IKLoopAngleLimit=0.0f; }
			}
			b->IKLinkNum=b->IKLinks.size();
		}

		//morphs: drop offsets that refer to nothing (and group morphs of group morphs)
		for(int i=0; i<morphCount; ++i)
		{
			PMXMorph *morph=m.morphs[i];
			if(morph->type>MORPH_TYPE_MATERIAL)
			{
				//no offsets of an unknown type can be kept
				note(true);
				for(PMXMorphData *d : morph->offsetData) delete static_cast<PMXUVMorph*>(d);
				morph->offsetData.clear();
				morph->type=MORPH_TYPE_VERTEX;
			}
			auto del=[&](PMXMorphData *d){ deleteMorphData(morph, d); };
			switch(morph->type)
			{
				case MORPH_TYPE_GROUP:
					fixes+=removeInvalid(morph->offsetData, [&](PMXMorphData *d){
						int t=static_cast<PMXGroupMorph*>(d)->morphIndex;
						return t<0 || t>=morphCount || t==i || m.morphs[t]->type==MORPH_TYPE_GROUP; }, del);
					break;
				case MORPH_TYPE_VERTEX:
					fixes+=removeInvalid(morph->offsetData, [&](PMXMorphData *d){
						int v=static_cast<PMXVertexMorph*>(d)->vertexIndex; return v<0 || v>=vertexCount; }, del);
					break;
				case MORPH_TYPE_BONE:
					fixes+=removeInvalid(morph->offsetData, [&](PMXMorphData *d){
						return !validBone(static_cast<PMXBoneMorph*>(d)->boneIndex); }, del);
					break;
				case MORPH_TYPE_MATERIAL:
					fixes+=removeInvalid(morph->offsetData, [&](PMXMorphData *d){
						int t=static_cast<PMXMaterialMorph*>(d)->materialIndex; return t<-1 || t>=materialCount; }, del);
					break;
				default: //UV, extra UV1-4
					fixes+=removeInvalid(morph->offsetData, [&](PMXMorphData *d){
						int v=static_cast<PMXUVMorph*>(d)->vertexIndex; return v<0 || v>=vertexCount; }, del);
					break;
			}
			morph->morphOffsetNum=morph->offsetData.size();
		}

		//display frames: drop elements that refer to nothing
		for(PMXDisplayFrame *f : m.displayFrames)
		{
			fixes+=removeInvalid(f->elements, [&](PMXDisplayFrameElement *e){
				return e->index>=(unsigned)(e->target ? morphCount : boneCount); });
			f->elementsWithinFrame=f->elements.size();
		}
		m.display_frame_continuing_datasets=m.displayFrames.size();

		//rigid bodies: bone, collision group (0-15), mode, shape, numbers
		//Bullet can work with
		for(PMXRigidBody *rb : m.rigidBodies)
		{
			if(rb->relatedBoneIndex!=-1 && !validBone(rb->relatedBoneIndex)) { note(true); rb->relatedBoneIndex=-1; }
			if(rb->group>15) { note(true); rb->group=0; }
			if(rb->physicsOperation>RIGID_TYPE_PHYSICS_ROT_ONLY) { note(true); rb->physicsOperation=RIGID_TYPE_PHYSICS; }
			if(rb->shape>RIGID_SHAPE_CAPSULE) { note(true); rb->shape=RIGID_SHAPE_SPHERE; }
			rb->size=glm::abs(finiteOr0(rb->size));
			rb->position=finiteOr0(rb->position);
			rb->rotation=finiteOr0(rb->rotation);
			rb->mass=std::fabs(finiteOr0(rb->mass));
			rb->movementDecay=finiteOr0(rb->movementDecay);
			rb->rotationDecay=finiteOr0(rb->rotationDecay);
			rb->elasticity=finiteOr0(rb->elasticity);
			rb->friction=finiteOr0(rb->friction);
		}

		//joints: rigid body A must exist, B is a rigid body or -1 (the world)
		const int rbCount=m.rigid_body_continuing_datasets;
		fixes+=removeInvalid(m.joints, [&](PMXJoint *j){
			int a=j->relatedRigidBodyIndexA, b=j->relatedRigidBodyIndexB;
			return a<0 || a>=rbCount || b<-1 || b>=rbCount || a==b; });
		m.joint_continuing_datasets=m.joints.size();
		for(PMXJoint *j : m.joints)
		{
			for(glm::vec3 *v : {&j->position, &j->rotation, &j->movementLowerLimit, &j->movementUpperLimit,
			                    &j->rotationLowerLimit, &j->rotationUpperLimit, &j->springMovementConstant, &j->springRotationConstant})
				*v=finiteOr0(*v);
		}

		if(fixes) cerr<<"WARNING: "<<fixes<<" invalid references in the model were fixed or removed"<<endl;
	}


	PMXInfo &readPMX(string foldername, string filename)
	{
		string fname = foldername + filename;
		std::vector<char> data=readWholeFile(fname, "PMX");
		if(data.size()>=3 && memcmp(data.data(), "Pmd", 3)==0)
		{
			//a PMD model (also accepted under a .pmx name)
			return readPMD(foldername, filename);
		}

		std::unique_ptr<PMXInfo> info(new PMXInfo());
		PMXInfo &pmxInfo = *info;
		BinaryReader r(data, BinaryReader::Mode::Throw, "PMX file", fname);

		//***Extract header info***
		r.raw(pmxInfo.header_str, 4);
		if(memcmp(pmxInfo.header_str, "PMX ", 4)!=0) r.fail("not a PMX file");
		pmxInfo.ver=r.f32();
		if (pmxInfo.ver != 2.0) {
			throwLoadError("Error: Only version 2.0 of the PMX file format is supported!");
		}
		cout<<"PMX Ver. "<<pmxInfo.ver<<endl;

		pmxInfo.line_size=r.u8();
		if(pmxInfo.line_size<8) r.fail("header size");
		uint8_t encoding=r.u8();
		if(encoding>PMX_ENCODE_UTF8) r.fail("text encoding");
		pmxInfo.unicode_type=encoding;
		pmxInfo.extraUVCount       = r.u8();
		pmxInfo.vertexIndexSize    = r.u8();
		pmxInfo.textureIndexSize   = r.u8();
		pmxInfo.materialIndexSize  = r.u8();
		pmxInfo.boneIndexSize      = r.u8();
		pmxInfo.morphIndexSize     = r.u8();
		pmxInfo.rigidBodyIndexSize = r.u8();
		r.skip(pmxInfo.line_size-8); //fields of later versions
		if(pmxInfo.extraUVCount>4) r.fail("extra UV count");
		for(uint8_t s : {pmxInfo.vertexIndexSize, pmxInfo.textureIndexSize, pmxInfo.materialIndexSize,
		                 pmxInfo.boneIndexSize, pmxInfo.morphIndexSize, pmxInfo.rigidBodyIndexSize})
		{
			if(!validIndexSize(s)) r.fail("index size");
		}
		const int vis=pmxInfo.vertexIndexSize, tis=pmxInfo.textureIndexSize, mis=pmxInfo.materialIndexSize;
		const int bis=pmxInfo.boneIndexSize, mos=pmxInfo.morphIndexSize, ris=pmxInfo.rigidBodyIndexSize;

		if (pmxInfo.unicode_type == PMX_ENCODE_UTF8) {
			cerr<<"WARNING: UTF-8 encoded PMX file loading is untested"<<endl;
		}

		//***Pull model name, comment info***
		pmxInfo.modelName=readPMXText(r, pmxInfo);
		pmxInfo.modelNameEnglish=readPMXText(r, pmxInfo);
		pmxInfo.comment=readPMXText(r, pmxInfo);
		pmxInfo.commentEnglish=readPMXText(r, pmxInfo);

		//***Pull Vertex Info***
		//Every count is checked against the rest of the file (smallest record
		//size) before anything is allocated for it.
		uint32_t vertexCount=r.checkCount((uint32_t)r.i32(), 12+12+8+16*pmxInfo.extraUVCount+1+bis+4, "vertex");
		pmxInfo.vertices.reserve(vertexCount);
		cout<<"Loading vertices...";
		for(uint32_t i = 0; i < vertexCount; ++i)
		{
			PMXVertex *vertex = new PMXVertex();
			pmxInfo.vertices.push_back(vertex);

			vertex->pos=readPos(r);
			vertex->normal=readPos(r);
			vertex->UV.x=r.f32();
			vertex->UV.y=r.f32();

			//***Additional UVs (0-4 vec4s)***
			for(int k = 0; k < 4; ++k) vertex->extraUV[k] = glm::vec4(0.0f);
			for(int k = 0; k < (int)pmxInfo.extraUVCount; ++k) vertex->extraUV[k]=readVec4(r);

			vertex->weight_transform_formula=r.u8();
			if (vertex->weight_transform_formula == WEIGHT_FORMULA_BDEF1) {
				vertex->boneIndex1=r.index(bis);
			} else if (vertex->weight_transform_formula == WEIGHT_FORMULA_BDEF2) {
				vertex->boneIndex1=r.index(bis);
				vertex->boneIndex2=r.index(bis);
				vertex->weight1=r.f32();
				vertex->weight2 = 1.0 - vertex->weight1; //For BDEF2: weight of bone2=1.0-weight1
			} else if (vertex->weight_transform_formula == WEIGHT_FORMULA_BDEF4) {
				vertex->boneIndex1=r.index(bis);
				vertex->boneIndex2=r.index(bis);
				vertex->boneIndex3=r.index(bis);
				vertex->boneIndex4=r.index(bis);
				vertex->weight1=r.f32();
				vertex->weight2=r.f32();
				vertex->weight3=r.f32();
				vertex->weight4=r.f32();
			} else if (vertex->weight_transform_formula == WEIGHT_FORMULA_SDEF) {
				vertex->boneIndex1=r.index(bis);
				vertex->boneIndex2=r.index(bis);
				vertex->weight1=r.f32();
				//convert to OpenGL (right-handed) coordinates like the vertex position
				vertex->C=readPos(r);
				vertex->R0=readPos(r);
				vertex->R1=readPos(r);
			} else {
				throwLoadError("ERROR: bone structure (QDEF?) not supported yet");
			}

			vertex->edgeScale=r.f32();
		}
		pmxInfo.vertex_continuing_datasets=vertexCount;
		cout<<"done."<<endl;

		//***Pull Face Info*** (a count of vertex indices, 3 per face)
		uint32_t indexCount=r.checkCount((uint32_t)r.i32(), vis, "face index");
		cout<<"Loading faces...";
		pmxInfo.faces.reserve(indexCount/3);
		for(uint32_t i = 0; i < indexCount / 3; i++)
		{
			PMXFace *face = new PMXFace();
			pmxInfo.faces.push_back(face);
			for(int k=0; k<3; ++k) face->points[k]=(unsigned)r.vertexIndex(vis);
		}
		r.skip((indexCount%3)*vis); //an incomplete last face
		pmxInfo.face_continuing_datasets=pmxInfo.faces.size();
		cout<<"done."<<endl;

		//***Pull Texture Info***
		uint32_t textureCount=r.checkCount((uint32_t)r.i32(), 4, "texture");
		pmxInfo.texturePaths = new string[textureCount + 11];
		pmxInfo.texture_continuing_datasets=textureCount;

		cout<<"Loading textures...";
		for(uint32_t i = 0; i < textureCount; ++i)
		{
			string &path=pmxInfo.texturePaths[i];
			path=readPMXText(r, pmxInfo);
			if(!foldername.empty())
			{
				path.insert(0, "/");
				path.insert(0, foldername);
			}
			std::replace(path.begin(), path.end(), '\\', '/');
		}
		cout<<"done."<<endl;

		//***Pull Material Info***
		uint32_t materialCount=r.checkCount((uint32_t)r.i32(), 4+4+16+12+4+12+1+16+4+2*tis+2+1+4+4, "material");
		cout<<"Loading materials...";
		for(uint32_t i = 0; i < materialCount; ++i)
		{
			PMXMaterial *material = new PMXMaterial();
			pmxInfo.materials.push_back(material);

			material->name=readPMXText(r, pmxInfo);
			material->nameEng=readPMXText(r, pmxInfo);
			material->diffuse=readVec4(r);
			material->specular=readVec3(r);
			material->shininess=r.f32();
			material->ambient=readVec3(r);

			uint8_t bitflag=r.u8();
			material->drawBothSides       = bitflag&0x01;
			material->drawGroundShadow    = bitflag&0x02;
			material->drawToSelfShadowMap = bitflag&0x04;
			material->drawSelfShadow      = bitflag&0x08;
			material->drawEdges           = bitflag&0x10;

			material->edgeColor=readVec4(r);
			material->edgeSize=r.f32();

			material->textureIndex=r.index(tis);
			material->sphereIndex=r.index(tis);
			material->sphereMode=r.u8();
			material->shareToon=r.u8();
			material->toonTextureIndex=-1;
			if (material->shareToon == 1) {
				material->shareToonTexture=r.u8();
			} else {
				material->toonTextureIndex=r.index(tis);
			}

			material->memo=readPMXText(r, pmxInfo);
			material->hasFaceNum=r.i32(); //number of indices that use the material
		}
		pmxInfo.material_continuing_datasets=materialCount;
		cout<<"done."<<endl;

		//***Pull Bone Info***
		uint32_t boneCount=r.checkCount((uint32_t)r.i32(), 4+4+12+bis+4+2+1, "bone");
		cout<<"Loading bones...";
		for(uint32_t i = 0; i < boneCount; ++i)
		{
			PMXBone *bone = new PMXBone();
			pmxInfo.bones.push_back(bone);

			bone->name=readPMXText(r, pmxInfo);
			bone->nameEng=readPMXText(r, pmxInfo);
			bone->position=readPos(r);

			//The parent may come LATER in the bone list (allowed by PMX), so the
			//pointer is resolved after all bones have been read.
			bone->parentBoneIndex=r.index(bis);
			bone->parent = NULL;
			bone->transformationLevel=r.i32();

			uint8_t flags1=r.u8(), flags2=r.u8();
			bone->connectionDisplayMethod = flags1&0x01;
			bone->rotationPossible        = flags1&0x02;
			bone->movementPossible        = flags1&0x04;
			bone->show                    = flags1&0x08;
			bone->controlPossible         = flags1&0x10;
			bone->IK                      = flags1&0x20;
			bone->giveRotation            = flags2&0x01;
			bone->giveTranslation         = flags2&0x02;
			bone->axisFixed               = flags2&0x04;
			bone->localAxis               = flags2&0x08;
			bone->transformAfterPhysics   = flags2&0x10;
			bone->externalParentTransform = flags2&0x20;

			bone->connectionBoneIndex=-1;
			bone->givenParentBoneIndex=-1;
			bone->IKTargetBoneIndex=-1;
			if (bone->connectionDisplayMethod == 0) {
				bone->coordinateOffset=readVec3(r); //0: Display with Coordinate Offset
			} else {
				bone->connectionBoneIndex=r.index(bis); //1: Display with Bone
			}
			if (bone->giveRotation || bone->giveTranslation) {
				bone->givenParentBoneIndex=r.index(bis);
				bone->giveRate=r.f32();
			}
			if (bone->axisFixed) {
				bone->axisDirectionVector=readVec3(r);
			}
			if (bone->localAxis) {
				bone->XAxisDirectionVector=readVec3(r);
				bone->ZAxisDirectionVector=readVec3(r);
			}
			if (bone->externalParentTransform) {
				bone->keyValue=r.i32();
			}
			if (bone->IK) {
				bone->IKTargetBoneIndex=r.index(bis);
				bone->IKLoopCount=r.u32();
				bone->IKLoopAngleLimit=r.f32();
				uint32_t linkCount=r.checkCount(r.u32(), bis+1, "IK link");
				for(uint32_t j = 0; j < linkCount; ++j)
				{
					PMXIKLink *link = new PMXIKLink();
					bone->IKLinks.push_back(link);
					link->linkBoneIndex=r.index(bis);
					link->angleLimit=r.flag();
					if (link->angleLimit) {
						link->lowerLimit=readVec3(r);
						link->upperLimit=readVec3(r);
						flipZAxisOfRotationalLimits(link->lowerLimit, link->upperLimit);
					}
				}
				bone->IKLinkNum=bone->IKLinks.size();
			}
		}
		pmxInfo.bone_continuing_datasets=boneCount;

		finalizePMXBones(pmxInfo);
		cout<<"done."<<endl;

		//***Pull Morph Info***
		uint32_t morphCount=r.checkCount((uint32_t)r.i32(), 4+4+1+1+4, "morph");
		cout<<"Loading morphs...";
		for(uint32_t m = 0; m < morphCount; ++m)
		{
			PMXMorph *morph = new PMXMorph;
			pmxInfo.morphs.push_back(morph);
			morph->name=readPMXText(r, pmxInfo);
			morph->nameEng=readPMXText(r, pmxInfo);
			morph->controlPanel=r.u8();
			morph->type=r.u8();
			morph->morphOffsetNum=0;

			size_t recordSize=0;
			switch(morph->type)
			{
				case MORPH_TYPE_GROUP:    recordSize=mos+4; break;
				case MORPH_TYPE_VERTEX:   recordSize=vis+12; break;
				case MORPH_TYPE_BONE:     recordSize=bis+28; break;
				case MORPH_TYPE_MATERIAL: recordSize=mis+1+28*4; break;
				case MORPH_TYPE_UV:
				case MORPH_TYPE_EXTRA_UV1:
				case MORPH_TYPE_EXTRA_UV2:
				case MORPH_TYPE_EXTRA_UV3:
				case MORPH_TYPE_EXTRA_UV4: recordSize=vis+16; break;
				default:
					throwLoadError("Unknown morph type or faulty data/reading");
			}
			uint32_t offsetCount=r.checkCount((uint32_t)r.i32(), recordSize, "morph offset");

			for(uint32_t i = 0; i < offsetCount; ++i)
			{
				switch (morph->type)
				{
					case MORPH_TYPE_VERTEX:
					{
						PMXVertexMorph *vertexMorph = new PMXVertexMorph();
						morph->offsetData.push_back(vertexMorph);
						vertexMorph->vertexIndex=r.vertexIndex(vis);
						vertexMorph->coordinateOffset=readPos(r);
					}
					break;

					case MORPH_TYPE_BONE:
					{
						PMXBoneMorph *boneMorph = new PMXBoneMorph();
						morph->offsetData.push_back(boneMorph);
						boneMorph->boneIndex=r.index(bis);
						boneMorph->inertia=readVec3(r);
						boneMorph->rotationAmount=readVec4(r);
						//Direct3D (left-handed) -> OpenGL (right-handed), as for VMD bone frames
						boneMorph->inertia.z = -boneMorph->inertia.z;
						boneMorph->rotationAmount.x = -boneMorph->rotationAmount.x;
						boneMorph->rotationAmount.y = -boneMorph->rotationAmount.y;
					}
					break;

					case MORPH_TYPE_MATERIAL:
					{
						PMXMaterialMorph *materialMorph = new PMXMaterialMorph();
						morph->offsetData.push_back(materialMorph);
						materialMorph->materialIndex=r.index(mis);
						materialMorph->offsetCalculationFormula=r.u8();
						materialMorph->diffuse=readVec4(r);
						materialMorph->specular=readVec3(r);
						materialMorph->shininess=r.f32();
						materialMorph->ambient=readVec3(r);
						materialMorph->edgeColor=readVec4(r);
						materialMorph->edgeSize=r.f32();
						materialMorph->textureCoefficient=readVec4(r);
						materialMorph->sphereTextureCoefficient=readVec4(r);
						materialMorph->toonTextureCoefficient=readVec4(r);
					}
					break;

					case MORPH_TYPE_GROUP:
					{
						PMXGroupMorph *groupMorph = new PMXGroupMorph();
						morph->offsetData.push_back(groupMorph);
						groupMorph->morphIndex=r.index(mos);
						groupMorph->morphRate=r.f32();
					}
					break;

					default: //UV, extra UV1-4
					{
						PMXUVMorph *UVMorph = new PMXUVMorph();
						morph->offsetData.push_back(UVMorph);
						UVMorph->vertexIndex=r.vertexIndex(vis);
						UVMorph->UVOffsetAmount=readVec4(r);
					}
					break;
				}
			}
			morph->morphOffsetNum=morph->offsetData.size();
		}
		pmxInfo.morph_continuing_datasets=morphCount;
		cout<<"done"<<endl;

		//***Pull Display Frame Info***
		uint32_t frameCount=r.checkCount((uint32_t)r.i32(), 4+4+1+4, "display frame");
		cout<<"Loading display frame...";
		for(uint32_t f = 0; f < frameCount; ++f)
		{
			PMXDisplayFrame *df = new PMXDisplayFrame();
			pmxInfo.displayFrames.push_back(df);
			df->name=readPMXText(r, pmxInfo);
			df->nameEng=readPMXText(r, pmxInfo);
			df->specialFrameFlag=r.flag();
			uint32_t elementCount=r.checkCount((uint32_t)r.i32(), 1+std::min(bis, mos), "display frame element");
			for(uint32_t i = 0; i < elementCount; ++i)
			{
				PMXDisplayFrameElement *element = new PMXDisplayFrameElement();
				df->elements.push_back(element);
				element->target=r.flag(); //0: bone, 1: morph
				element->index=(unsigned)r.index(element->target ? mos : bis);
			}
			df->elementsWithinFrame=df->elements.size();
		}
		pmxInfo.display_frame_continuing_datasets=frameCount;
		cout<<"done"<<endl;

		//***Pull Rigid Body Info***
		uint32_t rigidBodyCount=r.checkCount((uint32_t)r.i32(), 4+4+bis+1+2+1+36+20+1, "rigid body");
		cout<<"Loading rigid body...";
		for(uint32_t i = 0; i < rigidBodyCount; ++i)
		{
			PMXRigidBody *rb = new PMXRigidBody();
			pmxInfo.rigidBodies.push_back(rb);
			rb->name=readPMXText(r, pmxInfo);
			rb->nameEng=readPMXText(r, pmxInfo);
			rb->relatedBoneIndex=r.index(bis);
			rb->group=r.u8();
			rb->noCollisionGroupFlag=r.u16();
			rb->shape=r.u8();
			if(rb->shape>RIGID_SHAPE_CAPSULE) r.fail("rigid body shape");
			rb->size=readVec3(r);
			rb->position=readPos(r);
			rb->rotation=flipZAxisOfEulerAnglesRadians(readVec3(r));
			rb->mass=r.f32();
			rb->movementDecay=r.f32();
			rb->rotationDecay=r.f32();
			rb->elasticity=r.f32();
			rb->friction=r.f32();
			rb->physicsOperation=r.u8();
		}
		pmxInfo.rigid_body_continuing_datasets=rigidBodyCount;
		cout<<"done"<<endl;

		//***Pull Joint Info***
		cout<<"Loading joints...";
		uint32_t jointCount=r.checkCount((uint32_t)r.i32(), 4+4+1+2*ris+24*4, "joint");
		for(uint32_t i = 0; i < jointCount; ++i)
		{
			PMXJoint *joint = new PMXJoint;
			pmxInfo.joints.push_back(joint);
			joint->name=readPMXText(r, pmxInfo);
			joint->nameEng=readPMXText(r, pmxInfo);

			if(r.u8()!=0) //0: spring 6DOF, the only type in PMX 2.0
			{
				throwLoadError("Unsupported PMX format version or file reading error");
			}
			joint->type=0;
			joint->relatedRigidBodyIndexA=r.index(ris);
			joint->relatedRigidBodyIndexB=r.index(ris);
			joint->position=readPos(r);
			joint->rotation=flipZAxisOfEulerAnglesRadians(readVec3(r));
			joint->movementLowerLimit=readVec3(r);
			joint->movementUpperLimit=readVec3(r);
			joint->rotationLowerLimit=readVec3(r);
			joint->rotationUpperLimit=readVec3(r);
			flipZAxisOfRotationalLimits(joint->rotationLowerLimit, joint->rotationUpperLimit);
			joint->springMovementConstant=readVec3(r);
			joint->springRotationConstant=readVec3(r);
		}
		pmxInfo.joint_continuing_datasets=jointCount;
		cout<<"done"<<endl<<endl;

		sanitizePMXInfo(pmxInfo);

		return *info.release();
	}

	void printDebugInfo(PMXInfo &pmxInfo)
	{
		cout<<pmxInfo.header_str<<"ver "<<pmxInfo.ver<<endl;
		cout<<"line size: "<<(int)pmxInfo.line_size<<endl;
		cout<<"Unicode Type(0-UTF-16, 1-UTF-8): "<<pmxInfo.unicode_type<<endl;
		cout<<"Extra UV count: "<<(int)pmxInfo.extraUVCount<<endl;
		cout<<"Vertex Index Size: "<<(int)pmxInfo.vertexIndexSize<<endl;
		cout<<"Texture Index Size: "<<(int)pmxInfo.textureIndexSize<<endl;
		cout<<"Material Index Size: "<<(int)pmxInfo.materialIndexSize<<endl;
		cout<<"Bone Index Size: "<<(int)pmxInfo.boneIndexSize<<endl;
		cout<<"Morph Index Size: "<<(int)pmxInfo.morphIndexSize<<endl;
		cout<<"Rig Index Size: "<<(int)pmxInfo.rigidBodyIndexSize<<endl;
		cout<<endl<<endl;
		cout<<pmxInfo.modelName<<endl;
		cout<<pmxInfo.modelNameEnglish<<endl;
		cout<<endl;
		cout<<"Number of Continuing Vertex Datasets: "<<pmxInfo.vertex_continuing_datasets<<endl;
		
		for(int i = 0; i < pmxInfo.vertex_continuing_datasets; ++i)
		{
			PMXVertex *vertex = pmxInfo.vertices[i];
			
			if (VERTEX_DEBUG) cout<<"[Vertex "<<i<<"]"<<endl;
			if (VERTEX_DEBUG) cout<<"Position: "<<vertex->pos[0]<<" "<<vertex->pos[1]<<" "<<vertex->pos[2]<<endl<<endl;
			if (VERTEX_DEBUG) cout<<"Normal Vector: "<<vertex->normal[0]<<" "<<vertex->normal[1]<<" "<<vertex->normal[2]<<endl<<endl;
			if (VERTEX_DEBUG) cout<<vertex->UV[0]<<" "<<vertex->UV[1]<<" "<<endl<<endl;
			if (VERTEX_DEBUG) cout<<"Weight transform formula(0-BDEF1, 1-BDEF2, 2-BDEF4, 3-SDEF): "<<(int)vertex->weight_transform_formula<<endl;
			if (vertex->weight_transform_formula == WEIGHT_FORMULA_BDEF1) {
				if (VERTEX_DEBUG) cout<<"boneIndex1: "<<vertex->boneIndex1<<endl;
			} else if (vertex->weight_transform_formula == WEIGHT_FORMULA_BDEF2) {
				if (VERTEX_DEBUG) cout<<"boneIndex1: "<<vertex->boneIndex1<<endl;
				if (VERTEX_DEBUG) cout<<"boneIndex2: "<<vertex->boneIndex2<<endl;	
				if (VERTEX_DEBUG) cout<<"Weight1: "<<vertex->weight1<<endl;
				if (VERTEX_DEBUG) cout<<"Weight2: "<<vertex->weight2<<endl;
			} else if (vertex->weight_transform_formula == WEIGHT_FORMULA_BDEF4) {			
				if (VERTEX_DEBUG) cout<<"boneIndex1: "<<vertex->boneIndex1<<endl;
				if (VERTEX_DEBUG) cout<<"boneIndex1: "<<vertex->boneIndex2<<endl;
				if (VERTEX_DEBUG) cout<<"Weight1: "<<vertex->weight1<<endl;
				if (VERTEX_DEBUG) cout<<"Weight2: "<<vertex->weight2<<endl;
				if (VERTEX_DEBUG) cout<<"Weight3: "<<vertex->weight3<<endl;
				if (VERTEX_DEBUG) cout<<"Weight4: "<<vertex->weight4<<endl;
			} else if (vertex->weight_transform_formula == WEIGHT_FORMULA_SDEF) {			
				if (VERTEX_DEBUG) cout<<"boneIndex1: "<<vertex->boneIndex1<<endl;
				if (VERTEX_DEBUG) cout<<"boneIndex2: "<<vertex->boneIndex2<<endl;
				if (VERTEX_DEBUG) cout<<"Weight1: "<<vertex->weight1<<endl;
				if (VERTEX_DEBUG) cout<<"Weight2: "<<vertex->weight2<<endl;
				if (VERTEX_DEBUG) cout<<"C: "<<vertex->C[0]<<" "<<vertex->C[1]<<" "<<vertex->C[2]<<endl;
				if (VERTEX_DEBUG) cout<<"R0: "<<vertex->R0[0]<<" "<<vertex->R0[1]<<" "<<vertex->R0[2]<<endl;
				if (VERTEX_DEBUG) cout<<"R1: "<<vertex->R1[0]<<" "<<vertex->R1[1]<<" "<<vertex->R1[2]<<endl;
			}
			if(VERTEX_DEBUG) cout<<"edgeScale: "<<vertex->edgeScale<<endl<<endl;
		}
		
		cout<<"Number of Continuing Face Datasets: "<<pmxInfo.face_continuing_datasets<<endl;
		/*for(int i=0; i<pmxInfo.face_continuing_datasets; ++i)
		{
			PMXFace *face=pmxInfo.faces[i];
			cout<<"Vertex Index1: "<<face->points[0]<<endl;
			cout<<"Vertex Index2: "<<face->points[1]<<endl;
			cout<<"Vertex Index3: "<<face->points[2]<<endl;
		}*/

		cout<<"Number of Continuing Texture Datasets: "<<pmxInfo.texture_continuing_datasets<<endl;
		
		cout<<"Number of Continuing Material Datasets: "<<pmxInfo.material_continuing_datasets<<endl;
		for(int i = 0; i < pmxInfo.material_continuing_datasets; ++i)
		{		
			if (MATERIAL_DEBUG) cout<<"[Material "<<i<<"]"<<endl;
			
			PMXMaterial *material=pmxInfo.materials[i];
			
			if (MATERIAL_DEBUG) {
				cout<<"Diffuse: "<<endl;
				cout<<"R: "<<material->diffuse[0]<<endl;
				cout<<"G: "<<material->diffuse[1]<<endl;
				cout<<"B: "<<material->diffuse[2]<<endl;
				cout<<"A: "<<material->diffuse[3]<<endl<<endl;
			}
			if (MATERIAL_DEBUG) {
				cout<<"Specular: "<<endl;
				cout<<"R: "<<material->specular[0]<<endl;
				cout<<"G: "<<material->specular[1]<<endl;
				cout<<"B: "<<material->specular[2]<<endl;
			}
			if (MATERIAL_DEBUG) cout<<"Specular Coefficient: "<<material->shininess<<endl<<endl;
			if (MATERIAL_DEBUG) {
				cout<<"Edge Color: "<<endl;
				cout<<"R: "<<material->edgeColor[0]<<endl;
				cout<<"G: "<<material->edgeColor[1]<<endl;
				cout<<"B: "<<material->edgeColor[2]<<endl;
				cout<<"A: "<<material->edgeColor[3]<<endl;
			}
			if (MATERIAL_DEBUG) {
				cout<<"Ambient: "<<endl;
				cout<<"R: "<<material->ambient[0]<<endl;
				cout<<"G: "<<material->ambient[1]<<endl;
				cout<<"B: "<<material->ambient[2]<<endl;
			}
			if (MATERIAL_DEBUG) {
				cout<<"Draw Both Sides: "<<material->drawBothSides<<endl;
				cout<<"Draw Ground Shadow: "<<material->drawGroundShadow<<endl;
				cout<<"Draw To Self Shadow Map: "<<material->drawToSelfShadowMap<<endl;
				cout<<"Draw Self Shadow: "<<material->drawSelfShadow<<endl;
				cout<<"Draw Edges: "<<material->drawEdges<<endl<<endl;
			}
			if (MATERIAL_DEBUG) cout<<"Edge Size: "<<material->edgeSize<<endl<<endl;
			if (MATERIAL_DEBUG) cout<<"Sphere Mode: "<<(int)material->sphereMode<<endl;
			if (MATERIAL_DEBUG) cout<<"Share Toon: "<<(int)material->shareToon<<endl;
			if (MATERIAL_DEBUG) cout<<"Number of Indices that have this Material: "<<material->hasFaceNum<<endl;
			if (MATERIAL_DEBUG) {
				cout<<"Texture Index: "<<material->textureIndex<<endl;
				cout<<"Toon Texture Index: "<<material->toonTextureIndex<<endl;
				cout<<"Sphere Index: "<<material->sphereIndex<<endl<<endl;
			}
		}
		
		cout<<"Bone continuing datasets: "<<pmxInfo.bone_continuing_datasets<<endl;
		for(int i=0; i<pmxInfo.bone_continuing_datasets; ++i)
		{
			cout<<"\n[Bone "<<i<<"]"<<endl;
			cout<<*pmxInfo.bones[i]<<endl;
		}

		cout<<"RigidBody continuing datasets: "<<pmxInfo.rigid_body_continuing_datasets<<endl;
		for(int i = 0; i < pmxInfo.rigid_body_continuing_datasets; ++i)
		{
			cout<<"\n[RigidBody "<<i<<"]"<<endl;
			cout<<*pmxInfo.rigidBodies[i]<<endl;
		}

		cout<<"Joint continuing datasets: "<<pmxInfo.joint_continuing_datasets<<endl;
		for(int i = 0; i < pmxInfo.joint_continuing_datasets; ++i)
		{
			cout<<"\n[Joint "<<i<<"]"<<endl;
			cout<<*pmxInfo.joints[i]<<endl;
		}
	}

#define PRINT_MEMBER(inst, x) \
	{ rOut << #x << ": " << (inst).x << "\n"; }
#define PRINT_MEMBER_CAST(inst, x, type) \
	{ rOut << #x << ": " << (int)(inst).x << "\n"; }
	using ::operator<<;

	std::ostream&
	operator<<(std::ostream& rOut, const PMXIKLink& link)
	{
		PRINT_MEMBER(link, linkBoneIndex);
		PRINT_MEMBER(link, angleLimit);
		if (link.angleLimit)
		{
			PRINT_MEMBER(link, lowerLimit);
			PRINT_MEMBER(link, upperLimit);
		}
        return rOut;
	}

	std::ostream&
	operator<<(std::ostream& rOut, const PMXBone& bone)
	{
		PRINT_MEMBER(bone, name);
		PRINT_MEMBER(bone, nameEng);
		PRINT_MEMBER(bone, position);
		PRINT_MEMBER(bone, parentBoneIndex);
		PRINT_MEMBER(bone, transformationLevel);
		PRINT_MEMBER(bone, connectionDisplayMethod);
		PRINT_MEMBER(bone, rotationPossible);
		PRINT_MEMBER(bone, movementPossible);
		PRINT_MEMBER(bone, show);
		PRINT_MEMBER(bone, controlPossible);
		PRINT_MEMBER(bone, IK);
		PRINT_MEMBER(bone, giveRotation);
		PRINT_MEMBER(bone, giveTranslation);
		PRINT_MEMBER(bone, axisFixed);
		PRINT_MEMBER(bone, localAxis);
		PRINT_MEMBER(bone, transformAfterPhysics);
		PRINT_MEMBER(bone, externalParentTransform);
		if (bone.connectionDisplayMethod == 0)	//0: Display with Coordinate Offset
		{
			PRINT_MEMBER(bone, coordinateOffset);
		}
		else //1: Display with Bone
		{
			PRINT_MEMBER(bone, connectionBoneIndex);
		}

		if (bone.giveRotation || bone.giveTranslation)
		{
			PRINT_MEMBER(bone, givenParentBoneIndex);
			PRINT_MEMBER(bone, giveRate);
		}

		if (bone.axisFixed)
		{
			PRINT_MEMBER(bone, axisDirectionVector);
		}

		if (bone.localAxis)
		{
			PRINT_MEMBER(bone, XAxisDirectionVector);
			PRINT_MEMBER(bone, ZAxisDirectionVector);
		}

		if (bone.externalParentTransform)
		{
			PRINT_MEMBER(bone, keyValue);
		}

		if (bone.IK)
		{
			PRINT_MEMBER(bone, IKTargetBoneIndex);
			PRINT_MEMBER(bone, IKLoopCount);
			PRINT_MEMBER(bone, IKLoopAngleLimit);
			PRINT_MEMBER(bone, IKLinkNum);

			for (int j=0; j<bone.IKLinkNum; ++j)
			{
				rOut<<"\n[Link "<<j<<"]\n";
				rOut<<*bone.IKLinks[j]<<"\n";
			}
		}

		PRINT_MEMBER(bone, Local);
		return rOut;
	}

	std::ostream&
	operator<<(std::ostream& rOut, const PMXRigidBody& rb)
	{
		PRINT_MEMBER(rb, name);
		PRINT_MEMBER(rb, nameEng);
		PRINT_MEMBER(rb, relatedBoneIndex);
		PRINT_MEMBER_CAST(rb, group, int);
		PRINT_MEMBER(rb, noCollisionGroupFlag);
		PRINT_MEMBER_CAST(rb, shape, int);
		PRINT_MEMBER(rb, size);
		PRINT_MEMBER(rb, position);
		PRINT_MEMBER(rb, rotation);
		PRINT_MEMBER(rb, mass);
		PRINT_MEMBER(rb, movementDecay);
		PRINT_MEMBER(rb, rotationDecay);
		PRINT_MEMBER(rb, elasticity);
		PRINT_MEMBER(rb, friction);
		PRINT_MEMBER_CAST(rb, physicsOperation, int);
		PRINT_MEMBER(rb, Init);
		PRINT_MEMBER(rb, Offset);
		return rOut;
	}

	std::ostream&
	operator<<(std::ostream& rOut, const PMXJoint& jnt)
	{
		PRINT_MEMBER(jnt, name);
		PRINT_MEMBER(jnt, nameEng);
		PRINT_MEMBER(jnt, type);
		PRINT_MEMBER(jnt, relatedRigidBodyIndexA);
		PRINT_MEMBER(jnt, relatedRigidBodyIndexB);
		PRINT_MEMBER(jnt, position);
		PRINT_MEMBER(jnt, rotation);
		PRINT_MEMBER(jnt, movementLowerLimit);
		PRINT_MEMBER(jnt, movementUpperLimit);
		PRINT_MEMBER(jnt, rotationLowerLimit);
		PRINT_MEMBER(jnt, rotationUpperLimit);
		PRINT_MEMBER(jnt, springMovementConstant);
		PRINT_MEMBER(jnt, springRotationConstant);
		PRINT_MEMBER(jnt, Local);
		return rOut;
	}
} //end of namespace

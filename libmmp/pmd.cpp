//PMD (MikuMikuDance 1.0 model format) loader.
//
//The model is converted to the PMX structures used by the rest of libmmp,
//following the usual PMD -> PMX conventions (as PmxEditor and MMD do):
// - vertices become BDEF2, the PMD edge flag becomes the edge scale
// - "tex.bmp*sphere.sph" texture names are split into texture + sphere map
// - toon indices refer to the model's 10 toon file names; the standard
//   toon01.bmp-toon10.bmp become shared toons unless the model folder has
//   its own copy
// - IK bones get their data from the IK list (unit angle = control weight*4),
//   and "ひざ" links get MMD's knee angle limit
// - bone type 5 (回転影響下) and 9 (回転連動) become rotation inheritance,
//   type 8 (捩り) becomes a fixed axis
// - the base morph is dropped and the other vertex morphs are expanded to
//   absolute vertex indices
// - rigid body positions are relative to their bone in PMD
//Coordinates are converted from Direct3D (left-handed) to OpenGL exactly as
//in the PMX loader.

#include "pmx.h"
#include <stdexcept>
#include <sstream>
#include "glm_helper.h"
#include "texthandle.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <algorithm>

#include <dirent.h>
#include <strings.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;

namespace ClosedMMDFormat
{
	namespace
	{
		//Bounds-checked little-endian reader over the whole file
		class PMDReader
		{
			public:
			PMDReader(const vector<char> &data):d(data),pos(0),bad(false) {}

			bool ok() const { return !bad; }
			bool atEnd() const { return pos>=d.size(); }
			size_t remaining() const { return pos<d.size() ? d.size()-pos : 0; }

			void raw(void *out, size_t n)
			{
				if(bad || n>remaining())
				{
					bad=true;
					memset(out, 0, n);
					return;
				}
				memcpy(out, &d[pos], n);
				pos+=n;
			}
			uint8_t  u8()  { uint8_t v;  raw(&v,1); return v; }
			uint16_t u16() { uint16_t v; raw(&v,2); return v; }
			uint32_t u32() { uint32_t v; raw(&v,4); return v; }
			float    f32() { float v;    raw(&v,4); return v; }
			glm::vec3 vec3() { glm::vec3 v; v.x=f32(); v.y=f32(); v.z=f32(); return v; }
			glm::vec3 pos3() { glm::vec3 v=vec3(); v.z=-v.z; return v; } //D3D -> OpenGL

			//fixed-size Shift_JIS string (NUL-terminated within the field)
			string text(size_t n)
			{
				vector<char> buf(n+1, 0);
				raw(buf.data(), n);
				return sjisToUTF8(buf.data(), n);
			}
			//like text() but keeps the raw bytes (file names)
			string rawText(size_t n)
			{
				vector<char> buf(n+1, 0);
				raw(buf.data(), n);
				return string(buf.data(), strnlen(buf.data(), n));
			}
			void skip(size_t n)
			{
				if(n>remaining()) { bad=true; pos=d.size(); }
				else pos+=n;
			}

			private:
			const vector<char> &d;
			size_t pos;
			bool bad;
		};

		bool fail(const string &fname, const char *what)
		{
			{ std::ostringstream es_; es_<<"ERROR: corrupt or truncated PMD file ("<<what<<"): "<<fname; cerr<<es_.str()<<endl;
			throw std::runtime_error(es_.str()); }
			return false;
		}

		string trimRight(string s)
		{
			while(!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' ')) s.pop_back();
			return s;
		}

		string lowerExt(const string &name)
		{
			size_t dot=name.rfind('.');
			if(dot==string::npos) return "";
			string e=name.substr(dot+1);
			for(char &c : e) c=tolower((unsigned char)c);
			return e;
		}

		//Does the model folder contain this file (case-insensitively, as on Windows)?
		bool folderHasFile(const string &folder, const string &name)
		{
			DIR *dir=opendir(folder.empty() ? "." : folder.c_str());
			if(!dir) return false;
			bool found=false;
			while(dirent *e=readdir(dir))
			{
				if(strcasecmp(e->d_name, name.c_str())==0) { found=true; break; }
			}
			closedir(dir);
			return found;
		}

		//toon01.bmp-toon10.bmp -> 0-9, otherwise -1
		int sharedToonIndex(const string &name)
		{
			if(name.size()!=10 || strncasecmp(name.c_str(), "toon", 4)!=0 || strcasecmp(name.c_str()+6, ".bmp")!=0) return -1;
			if(!isdigit((unsigned char)name[4]) || !isdigit((unsigned char)name[5])) return -1;
			int n=(name[4]-'0')*10+(name[5]-'0');
			return (n>=1 && n<=10) ? n-1 : -1;
		}

		bool contains(const string &s, const char *utf8)
		{
			return s.find(utf8)!=string::npos;
		}
	}

	PMXInfo &readPMD(string foldername, string filename)
	{
		string fname=foldername+filename;
		ifstream file(fname.c_str(), ios::in | ios::binary);
		if(!file)
		{
			{ std::ostringstream es_; es_<<"ERROR: PMD file could not be found: "<<fname; cerr<<es_.str()<<endl;
			throw std::runtime_error(es_.str()); }
		}
		vector<char> data((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
		file.close();

		PMDReader r(data);
		PMXInfo *pInfo=new PMXInfo();
		PMXInfo &info=*pInfo;

		//***Header***
		char magic[3];
		r.raw(magic, 3);
		if(!r.ok() || memcmp(magic, "Pmd", 3)!=0)
		{
			{ std::ostringstream es_; es_<<"ERROR: not a PMD file: "<<fname; cerr<<es_.str()<<endl;
			throw std::runtime_error(es_.str()); }
		}
		memcpy(info.header_str, "Pmd", 4);
		info.ver=r.f32();
		cout<<"PMD Ver. "<<info.ver<<endl;
		info.line_size=8;
		info.unicode_type=PMX_ENCODE_UTF8; //names are converted to UTF-8
		info.extraUVCount=0;
		info.vertexIndexSize=info.textureIndexSize=info.materialIndexSize=4;
		info.boneIndexSize=info.morphIndexSize=info.rigidBodyIndexSize=4;

		info.modelName=r.text(20);
		info.comment=r.text(256);

		//***Vertices***
		uint32_t vertexCount=r.u32();
		if(!r.ok() || vertexCount>r.remaining()/38) fail(fname, "vertex count");
		info.vertex_continuing_datasets=vertexCount;
		cout<<"Loading vertices...";
		for(uint32_t i=0; i<vertexCount; ++i)
		{
			PMXVertex *v=new PMXVertex();
			v->pos=r.pos3();
			v->normal=r.pos3();
			v->UV.x=r.f32();
			v->UV.y=r.f32();
			v->boneIndex1=r.u16();
			v->boneIndex2=r.u16();
			//bone indices are validated once the bone count is known
			uint8_t weight=r.u8();
			uint8_t noEdge=r.u8();

			v->weight_transform_formula=WEIGHT_FORMULA_BDEF2;
			v->weight1=glm::clamp(weight/100.0f, 0.0f, 1.0f);
			v->weight2=1.0f-v->weight1;
			v->boneIndex3=v->boneIndex4=0;
			v->weight3=v->weight4=0.0f;
			v->edgeScale=noEdge ? 0.0f : 1.0f;
			info.vertices.push_back(v);
		}
		cout<<"done."<<endl;

		//***Faces***
		uint32_t indexCount=r.u32();
		if(!r.ok() || indexCount>r.remaining()/2) fail(fname, "face count");
		info.face_continuing_datasets=indexCount/3;
		for(uint32_t i=0; i+2<indexCount; i+=3)
		{
			PMXFace *f=new PMXFace();
			for(int k=0; k<3; ++k)
			{
				unsigned idx=r.u16();
				if(idx>=vertexCount) idx=0;
				f->points[k]=idx;
			}
			info.faces.push_back(f);
		}
		r.skip((indexCount%3)*2);

		//***Materials*** (texture names are resolved after the toon list is known)
		struct RawMaterial { string texture; uint8_t toon; };
		vector<RawMaterial> rawMaterials;
		uint32_t materialCount=r.u32();
		if(!r.ok() || materialCount>r.remaining()/70) fail(fname, "material count");
		info.material_continuing_datasets=materialCount;
		cout<<"Loading materials...";
		for(uint32_t i=0; i<materialCount; ++i)
		{
			PMXMaterial *m=new PMXMaterial();
			m->diffuse.r=r.f32(); m->diffuse.g=r.f32(); m->diffuse.b=r.f32(); m->diffuse.a=r.f32();
			m->shininess=r.f32();
			m->specular=r.vec3();
			m->ambient=r.vec3();
			RawMaterial raw;
			raw.toon=r.u8();
			uint8_t edge=r.u8();
			m->hasFaceNum=r.u32();
			raw.texture=r.rawText(20);
			rawMaterials.push_back(raw);

			char name[32];
			snprintf(name, sizeof(name), "材質%u", i+1);
			m->name=name;

			//MMD draws PMD materials with alpha < 1 without culling; the
			//edge flag also controls the ground shadow; alpha 0.98 turns
			//the self shadow off.
			m->drawBothSides=m->diffuse.a<1.0f;
			m->drawGroundShadow=edge!=0;
			m->drawToSelfShadowMap=m->diffuse.a!=0.98f;
			m->drawSelfShadow=m->diffuse.a!=0.98f;
			m->drawEdges=edge!=0;
			m->edgeColor=glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			m->edgeSize=1.0f;
			m->textureIndex=-1;
			m->sphereIndex=-1;
			m->sphereMode=0;
			m->shareToon=0;
			m->toonTextureIndex=-1;
			m->shareToonTexture=0;
			info.materials.push_back(m);
		}
		cout<<"done."<<endl;

		//***Bones***
		struct RawBone { uint16_t parent, tail, ikParent; uint8_t type; };
		vector<RawBone> rawBones;
		uint16_t boneCount=r.u16();
		if(!r.ok() || boneCount>r.remaining()/39) fail(fname, "bone count");
		info.bone_continuing_datasets=boneCount;
		cout<<"Loading bones...";
		for(int i=0; i<boneCount; ++i)
		{
			PMXBone *b=new PMXBone();
			b->name=r.text(20);
			RawBone raw;
			raw.parent=r.u16();
			raw.tail=r.u16();
			raw.type=r.u8();
			raw.ikParent=r.u16();
			b->position=r.pos3();
			rawBones.push_back(raw);
			info.bones.push_back(b);
		}
		for(int i=0; i<boneCount; ++i)
		{
			PMXBone *b=info.bones[i];
			const RawBone &raw=rawBones[i];
			b->parentBoneIndex=(raw.parent<boneCount) ? raw.parent : -1;
			b->parent=NULL;
			b->transformationLevel=0;

			//types: 0 rotate, 1 rotate+move, 2 IK, 3 unknown, 4 under IK,
			//5 under rotation, 6 IK target, 7 hidden, 8 twist, 9 rotation link
			b->rotationPossible=true;
			b->movementPossible=(raw.type==1 || raw.type==2);
			b->show=!(raw.type==6 || raw.type==7 || raw.type==9);
			b->controlPossible=!(raw.type==4 || raw.type==5 || raw.type==6 || raw.type==7 || raw.type==9);
			b->IK=false; //set from the IK list below
			b->giveRotation=false;
			b->giveTranslation=false;
			b->axisFixed=false;
			b->localAxis=false;
			b->transformAfterPhysics=false;
			b->externalParentTransform=false;
			b->givenParentBoneIndex=-1;
			b->giveRate=0.0f;
			b->IKTargetBoneIndex=-1;
			b->IKLoopCount=0;
			b->IKLoopAngleLimit=0.0f;
			b->IKLinkNum=0;

			//type 9 stores the influence (%) in the IK parent field, so its
			//tail field is the linked bone, not a display tail
			if(raw.type!=9 && raw.tail>0 && raw.tail<boneCount)
			{
				b->connectionDisplayMethod=true;
				b->connectionBoneIndex=raw.tail;
			}
			else
			{
				b->connectionDisplayMethod=false;
				b->coordinateOffset=glm::vec3(0.0f);
				b->connectionBoneIndex=-1;
			}

			if(raw.type==5 && raw.ikParent<boneCount && raw.ikParent!=i)
			{
				//回転影響下: follows the rotation of the "IK parent" bone
				b->giveRotation=true;
				b->givenParentBoneIndex=raw.ikParent;
				b->giveRate=1.0f;
			}
			else if(raw.type==9 && raw.tail<boneCount && raw.tail!=i)
			{
				//回転連動: tail = linked bone, IK parent field = rate in percent
				b->giveRotation=true;
				b->givenParentBoneIndex=raw.tail;
				b->giveRate=raw.ikParent*0.01f;
			}
			else if(raw.type==8 && raw.tail>0 && raw.tail<boneCount)
			{
				//捩り: rotation about the bone -> tail axis only
				glm::vec3 axis=info.bones[raw.tail]->position-b->position;
				if(glm::length(axis)>1.0e-6f)
				{
					b->axisFixed=true;
					b->axisDirectionVector=glm::normalize(axis);
				}
			}
		}

		//***IK***
		uint16_t ikCount=r.u16();
		if(!r.ok()) fail(fname, "IK count");
		for(int i=0; i<ikCount; ++i)
		{
			uint16_t boneIndex=r.u16();
			uint16_t target=r.u16();
			uint8_t chainLength=r.u8();
			uint16_t iterations=r.u16();
			float controlWeight=r.f32();
			vector<uint16_t> chain(chainLength);
			for(int k=0; k<chainLength; ++k) chain[k]=r.u16();
			if(!r.ok()) fail(fname, "IK list");
			if(boneIndex>=boneCount || target>=boneCount) continue;

			PMXBone *b=info.bones[boneIndex];
			b->IK=true;
			b->IKTargetBoneIndex=target;
			b->IKLoopCount=iterations;
			b->IKLoopAngleLimit=controlWeight*4.0f; //PMX unit angle = PMD control weight * 4
			for(int k=0; k<chainLength; ++k)
			{
				if(chain[k]>=boneCount) continue;
				PMXIKLink *link=new PMXIKLink();
				link->linkBoneIndex=chain[k];
				link->angleLimit=false;
				link->lowerLimit=link->upperLimit=glm::vec3(0.0f);
				if(contains(info.bones[chain[k]]->name, u8"ひざ"))
				{
					//MMD treats PMD knees as hinges bending one way only
					link->angleLimit=true;
					link->lowerLimit=glm::vec3((float)(-M_PI), 0.0f, 0.0f);
					link->upperLimit=glm::vec3((float)(-0.5*M_PI/180.0), 0.0f, 0.0f);
					flipZAxisOfRotationalLimits(link->lowerLimit, link->upperLimit);
				}
				b->IKLinks.push_back(link);
			}
			b->IKLinkNum=b->IKLinks.size();
		}
		finalizePMXBones(info);
		for(PMXVertex *v : info.vertices)
		{
			if(v->boneIndex1<0 || v->boneIndex1>=boneCount) v->boneIndex1=0;
			if(v->boneIndex2<0 || v->boneIndex2>=boneCount) v->boneIndex2=0;
		}
		cout<<"done."<<endl;

		//***Morphs (skins)***
		cout<<"Loading morphs...";
		uint16_t skinCount=r.u16();
		if(!r.ok()) fail(fname, "morph count");
		vector<int> baseIndices; //base morph: vertex index of every morph entry
		vector<int> skinToMorph(skinCount, -1);
		for(int i=0; i<skinCount; ++i)
		{
			string name=r.text(20);
			uint32_t count=r.u32();
			uint8_t type=r.u8();
			if(!r.ok() || count>r.remaining()/16) fail(fname, "morph data");

			if(type==0) //base
			{
				baseIndices.resize(count);
				for(uint32_t k=0; k<count; ++k)
				{
					uint32_t idx=r.u32();
					r.skip(12);
					baseIndices[k]=(idx<vertexCount) ? (int)idx : -1;
				}
				continue;
			}

			PMXMorph *m=new PMXMorph();
			m->name=name;
			m->controlPanel=type;
			m->type=MORPH_TYPE_VERTEX;
			for(uint32_t k=0; k<count; ++k)
			{
				uint32_t idx=r.u32();
				glm::vec3 offset=r.pos3();
				if(idx>=baseIndices.size() || baseIndices[idx]<0) continue;
				PMXVertexMorph *vm=new PMXVertexMorph();
				vm->vertexIndex=baseIndices[idx];
				vm->coordinateOffset=offset;
				m->offsetData.push_back(vm);
			}
			m->morphOffsetNum=m->offsetData.size();
			skinToMorph[i]=info.morphs.size();
			info.morphs.push_back(m);
		}
		info.morph_continuing_datasets=info.morphs.size();
		cout<<"done"<<endl;

		//***Display lists*** -> PMX display frames
		{
			PMXDisplayFrame *root=new PMXDisplayFrame();
			root->name="Root";
			root->nameEng="Root";
			root->specialFrameFlag=true;
			if(boneCount>0)
			{
				PMXDisplayFrameElement *e=new PMXDisplayFrameElement();
				e->target=false;
				e->index=0;
				root->elements.push_back(e);
			}
			root->elementsWithinFrame=root->elements.size();
			info.displayFrames.push_back(root);

			PMXDisplayFrame *exp=new PMXDisplayFrame();
			exp->name=u8"表情";
			exp->nameEng="Exp";
			exp->specialFrameFlag=true;
			uint8_t skinDispCount=r.u8();
			for(int i=0; i<skinDispCount; ++i)
			{
				uint16_t s=r.u16();
				if(s<skinCount && skinToMorph[s]>=0)
				{
					PMXDisplayFrameElement *e=new PMXDisplayFrameElement();
					e->target=true;
					e->index=skinToMorph[s];
					exp->elements.push_back(e);
				}
			}
			exp->elementsWithinFrame=exp->elements.size();
			info.displayFrames.push_back(exp);
		}
		uint8_t boneDispNameCount=r.u8();
		size_t firstBoneFrame=info.displayFrames.size();
		for(int i=0; i<boneDispNameCount; ++i)
		{
			PMXDisplayFrame *f=new PMXDisplayFrame();
			f->name=trimRight(r.text(50));
			f->specialFrameFlag=false;
			f->elementsWithinFrame=0;
			info.displayFrames.push_back(f);
		}
		uint32_t boneDispCount=r.u32();
		if(!r.ok() || boneDispCount>r.remaining()/3) fail(fname, "bone display list");
		for(uint32_t i=0; i<boneDispCount; ++i)
		{
			uint16_t bone=r.u16();
			uint8_t frame=r.u8(); //1-based index into the display names
			if(bone>=boneCount || frame<1 || frame>boneDispNameCount) continue;
			PMXDisplayFrameElement *e=new PMXDisplayFrameElement();
			e->target=false;
			e->index=bone;
			PMXDisplayFrame *f=info.displayFrames[firstBoneFrame+frame-1];
			f->elements.push_back(e);
			f->elementsWithinFrame=f->elements.size();
		}
		info.display_frame_continuing_datasets=info.displayFrames.size();
		if(!r.ok()) fail(fname, "display lists");

		//***Optional extensions (older PMD files may end here)***
		//English names
		if(!r.atEnd())
		{
			uint8_t hasEnglish=r.u8();
			if(hasEnglish)
			{
				info.modelNameEnglish=r.text(20);
				info.commentEnglish=r.text(256);
				for(int i=0; i<boneCount; ++i) info.bones[i]->nameEng=r.text(20);
				for(int i=0; i<skinCount; ++i)
				{
					if(i==0) continue; //no English name for the base morph
					string n=r.text(20);
					if(skinToMorph[i]>=0) info.morphs[skinToMorph[i]]->nameEng=n;
				}
				for(int i=0; i<boneDispNameCount; ++i)
					info.displayFrames[firstBoneFrame+i]->nameEng=trimRight(r.text(50));
			}
		}

		//Toon texture names (default: toon01.bmp-toon10.bmp)
		string toonNames[10];
		for(int i=0; i<10; ++i)
		{
			char n[16];
			snprintf(n, sizeof(n), "toon%02d.bmp", i+1);
			toonNames[i]=n;
		}
		if(r.remaining()>=1000)
		{
			for(int i=0; i<10; ++i) toonNames[i]=r.rawText(100);
		}

		//***Textures***
		vector<string> texturePaths;
		auto addTexture=[&](const string &name)->int
		{
			string path=name;
			replace(path.begin(), path.end(), '\\', '/');
			if(!foldername.empty()) path=foldername+"/"+path;
			for(size_t i=0; i<texturePaths.size(); ++i)
			{
				if(texturePaths[i]==path) return (int)i;
			}
			texturePaths.push_back(path);
			return (int)texturePaths.size()-1;
		};

		for(uint32_t i=0; i<materialCount; ++i)
		{
			PMXMaterial *m=info.materials[i];
			const string &tex=rawMaterials[i].texture;

			//"texture*sphere", or a single texture / sphere map
			string first=tex, second;
			size_t star=tex.find('*');
			if(star!=string::npos)
			{
				first=tex.substr(0, star);
				second=tex.substr(star+1);
			}
			for(const string *part : {&first, &second})
			{
				if(part->empty()) continue;
				string ext=lowerExt(*part);
				if(ext=="sph" || ext=="spa")
				{
					m->sphereIndex=addTexture(*part);
					m->sphereMode=(ext=="sph") ? 1 : 2;
				}
				else
				{
					m->textureIndex=addTexture(*part);
				}
			}

			//toon: 0xFF = none, otherwise an index into the toon list
			uint8_t t=rawMaterials[i].toon;
			if(t<10)
			{
				const string &toon=toonNames[t];
				int shared=sharedToonIndex(toon);
				if(shared>=0 && !folderHasFile(foldername, toon))
				{
					m->shareToon=1;
					m->shareToonTexture=shared;
				}
				else if(!toon.empty())
				{
					m->shareToon=0;
					m->toonTextureIndex=addTexture(toon);
				}
			}
		}
		info.texture_continuing_datasets=texturePaths.size();
		info.texturePaths=new string[texturePaths.size()+11];
		for(size_t i=0; i<texturePaths.size(); ++i) info.texturePaths[i]=texturePaths[i];

		//***Rigid bodies***
		info.rigid_body_continuing_datasets=0;
		info.joint_continuing_datasets=0;
		if(r.remaining()>=4)
		{
			cout<<"Loading rigid body...";
			uint32_t rbCount=r.u32();
			if(rbCount>r.remaining()/83) fail(fname, "rigid body count");

			//PMD rigid body positions are relative to their bone (センター if none)
			int centerBone=boneCount>0 ? 0 : -1;
			for(int i=0; i<boneCount; ++i)
			{
				if(info.bones[i]->name==u8"センター") { centerBone=i; break; }
			}

			for(uint32_t i=0; i<rbCount; ++i)
			{
				PMXRigidBody *rb=new PMXRigidBody();
				rb->name=r.text(20);
				uint16_t bone=r.u16();
				rb->group=r.u8();
				rb->noCollisionGroupFlag=r.u16();
				rb->shape=r.u8();
				rb->size=r.vec3();
				glm::vec3 pos=r.pos3();
				rb->rotation=flipZAxisOfEulerAnglesRadians(r.vec3());
				rb->mass=r.f32();
				rb->movementDecay=r.f32();
				rb->rotationDecay=r.f32();
				rb->elasticity=r.f32();
				rb->friction=r.f32();
				rb->physicsOperation=r.u8();

				//Without a bone (0xFFFF) the position is relative to センター, but
				//the body is not attached to it (it must not move the model).
				int origin=(bone<boneCount) ? (int)bone : centerBone;
				rb->relatedBoneIndex=(bone<boneCount) ? (int)bone : -1;
				rb->position=pos + (origin>=0 ? info.bones[origin]->position : glm::vec3(0.0f));
				info.rigidBodies.push_back(rb);
			}
			info.rigid_body_continuing_datasets=info.rigidBodies.size();
			cout<<"done"<<endl;

			//***Joints***
			if(r.remaining()>=4)
			{
				uint32_t jointCount=r.u32();
				if(jointCount>r.remaining()/124) fail(fname, "joint count");
				cout<<"Loading joints...";
				for(uint32_t i=0; i<jointCount; ++i)
				{
					PMXJoint *j=new PMXJoint();
					j->name=r.text(20);
					j->type=0;
					uint32_t a=r.u32(), b=r.u32();
					j->relatedRigidBodyIndexA=(a<rbCount) ? (int)a : -1;
					j->relatedRigidBodyIndexB=(b<rbCount) ? (int)b : -1;
					j->position=r.pos3();
					j->rotation=flipZAxisOfEulerAnglesRadians(r.vec3());
					j->movementLowerLimit=r.vec3();
					j->movementUpperLimit=r.vec3();
					j->rotationLowerLimit=r.vec3();
					j->rotationUpperLimit=r.vec3();
					flipZAxisOfRotationalLimits(j->rotationLowerLimit, j->rotationUpperLimit);
					j->springMovementConstant=r.vec3();
					j->springRotationConstant=r.vec3();
					info.joints.push_back(j);
				}
				info.joint_continuing_datasets=info.joints.size();
				cout<<"done"<<endl;
			}
		}
		if(!r.ok()) fail(fname, "physics");
		cout<<endl;

		return info;
	}
}

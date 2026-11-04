#include "fuzz_common.h"

#include "motioncontroller.h"
#include "bulletphysics.h"
#include "mmdphysics.h"
#include "cameracontroller.h"
#include "lightcontroller.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <cstdio>
#include <cstdlib>

#include <sys/mman.h>
#include <unistd.h>

using namespace ClosedMMDFormat;

namespace mmpfuzz
{
	void installDummyGL()
	{
		epoxy_glGetUniformLocation=[](GLuint, const GLchar*) -> GLint { return 0; };
		epoxy_glGenBuffers=[](GLsizei n, GLuint *b) { for(GLsizei i=0; i<n; ++i) b[i]=1; };
		epoxy_glGenTextures=[](GLsizei n, GLuint *t) { for(GLsizei i=0; i<n; ++i) t[i]=1; };
		epoxy_glDeleteBuffers=[](GLsizei, const GLuint*) {};
		epoxy_glDeleteTextures=[](GLsizei, const GLuint*) {};
		epoxy_glBindBuffer=[](GLenum, GLuint) {};
		epoxy_glBindTexture=[](GLenum, GLuint) {};
		epoxy_glActiveTexture=[](GLenum) {};
		epoxy_glTexBuffer=[](GLenum, GLenum, GLuint) {};
		epoxy_glUniform1i=[](GLint, GLint) {};
		epoxy_glGetIntegerv=[](GLenum, GLint *v) { *v=0; };
		//read the uploaded data, so that ASan sees out-of-bounds sizes
		epoxy_glBufferData=[](GLenum, GLsizeiptr size, const void *data, GLenum)
		{
			volatile unsigned char sink=0;
			if(data && size>0) { sink^=((const unsigned char*)data)[0]; sink^=((const unsigned char*)data)[size-1]; }
		};
		epoxy_glBufferSubData=[](GLenum, GLintptr, GLsizeiptr size, const void *data)
		{
			volatile unsigned char sink=0;
			if(data && size>0) { sink^=((const unsigned char*)data)[0]; sink^=((const unsigned char*)data)[size-1]; }
		};
	}

	void silenceOutput()
	{
		static std::ostringstream sink;
		sink.setstate(std::ios::badbit); //discard everything
		std::cout.rdbuf(sink.rdbuf());
		std::cerr.rdbuf(sink.rdbuf());
	}

	std::string memoryFile(const uint8_t *data, size_t size)
	{
		static int fd=-1;
		if(fd<0)
		{
			fd=memfd_create("mmp-fuzz", MFD_CLOEXEC);
			if(fd<0) { perror("memfd_create"); abort(); }
		}
		if(ftruncate(fd, 0)!=0) { perror("ftruncate"); abort(); }
		size_t done=0;
		while(done<size)
		{
			ssize_t n=pwrite(fd, data+done, size-done, done);
			if(n<=0) { perror("pwrite"); abort(); }
			done+=n;
		}
		return "/proc/self/fd/"+std::to_string(fd);
	}

	//***built-in model***
	static PMXBone *bone(PMXInfo &m, const char *name, glm::vec3 pos, int parent)
	{
		PMXBone *b=new PMXBone();
		b->name=name;
		b->position=pos;
		b->parentBoneIndex=parent;
		b->rotationPossible=b->movementPossible=b->show=b->controlPossible=true;
		b->connectionBoneIndex=-1;
		b->givenParentBoneIndex=-1;
		b->IKTargetBoneIndex=-1;
		m.bones.push_back(b);
		return b;
	}

	static PMXMorph *morph(PMXInfo &m, const char *name, uint8_t type)
	{
		PMXMorph *mo=new PMXMorph();
		mo->name=name;
		mo->controlPanel=4;
		mo->type=type;
		m.morphs.push_back(mo);
		return mo;
	}

	static PMXRigidBody *rigidBody(PMXInfo &m, int bone, uint8_t shape, uint8_t mode, glm::vec3 pos)
	{
		PMXRigidBody *rb=new PMXRigidBody();
		rb->name="rb";
		rb->relatedBoneIndex=bone;
		rb->group=(uint8_t)(m.rigidBodies.size());
		rb->noCollisionGroupFlag=0xFFFF;
		rb->shape=shape;
		rb->size=glm::vec3(1.0f, 2.0f, 1.0f);
		rb->position=pos;
		rb->mass=1.0f;
		rb->movementDecay=rb->rotationDecay=0.5f;
		rb->elasticity=0.0f;
		rb->friction=0.5f;
		rb->physicsOperation=mode;
		m.rigidBodies.push_back(rb);
		return rb;
	}

	PMXInfo *makeTestModel()
	{
		PMXInfo *pm=new PMXInfo();
		PMXInfo &m=*pm;
		memcpy(m.header_str, "PMX ", 4);
		m.ver=2.0f;
		m.line_size=8;
		m.unicode_type=PMX_ENCODE_UTF8;
		m.extraUVCount=1;
		m.vertexIndexSize=m.textureIndexSize=m.materialIndexSize=4;
		m.boneIndexSize=m.morphIndexSize=m.rigidBodyIndexSize=4;
		m.modelName="test";

		//bones: 0 センター, 1 上半身, 2 左足, 3 左ひざ, 4 左足首, 5 左足ＩＫ,
		//6 付与 (rotation+translation inheritance), 7 髪 (physics), 8 後 (after physics)
		bone(m, u8"センター", glm::vec3(0, 8, 0), -1);
		bone(m, u8"上半身", glm::vec3(0, 12, 0), 0);
		bone(m, u8"左足", glm::vec3(1, 10, 0), 0);
		bone(m, u8"左ひざ", glm::vec3(1, 5, 0), 2);
		bone(m, u8"左足首", glm::vec3(1, 1, 0), 3);
		PMXBone *ik=bone(m, u8"左足ＩＫ", glm::vec3(1, 1, 0), -1);
		ik->IK=true;
		ik->IKTargetBoneIndex=4;
		ik->IKLoopCount=40;
		ik->IKLoopAngleLimit=2.0f;
		{
			PMXIKLink *knee=new PMXIKLink();
			knee->linkBoneIndex=3;
			knee->angleLimit=true;
			knee->lowerLimit=glm::vec3(-3.14f, 0, 0);
			knee->upperLimit=glm::vec3(-0.01f, 0, 0);
			ik->IKLinks.push_back(knee);
			PMXIKLink *leg=new PMXIKLink();
			leg->linkBoneIndex=2;
			ik->IKLinks.push_back(leg);
			ik->IKLinkNum=ik->IKLinks.size();
		}
		PMXBone *give=bone(m, u8"付与", glm::vec3(0, 13, 0), 1);
		give->giveRotation=give->giveTranslation=true;
		give->givenParentBoneIndex=1;
		give->giveRate=0.5f;
		bone(m, u8"髪", glm::vec3(0, 14, -1), 1);
		PMXBone *after=bone(m, u8"後", glm::vec3(0, 15, 0), 1);
		after->transformAfterPhysics=true;
		after->transformationLevel=1;
		m.bone_continuing_datasets=m.bones.size();
		finalizePMXBones(m);

		//vertices: one quad per bone pair, all weight types
		for(int i=0; i<8; ++i)
		{
			PMXVertex *v=new PMXVertex();
			v->pos=glm::vec3(i%2, i, 0);
			v->normal=glm::vec3(0, 0, 1);
			v->UV=glm::vec2(i%2, i/8.0f);
			v->extraUV[0]=glm::vec4(0.5f);
			v->weight_transform_formula=i%4;
			v->boneIndex1=i%m.bone_continuing_datasets;
			v->boneIndex2=(i+1)%m.bone_continuing_datasets;
			v->boneIndex3=(i+2)%m.bone_continuing_datasets;
			v->boneIndex4=(i+3)%m.bone_continuing_datasets;
			v->weight1=0.4f; v->weight2=0.3f; v->weight3=0.2f; v->weight4=0.1f;
			v->C=glm::vec3(0, i, 0); v->R0=glm::vec3(0, i+1, 0); v->R1=glm::vec3(0, i-1, 0);
			v->edgeScale=1.0f;
			m.vertices.push_back(v);
		}
		m.vertex_continuing_datasets=m.vertices.size();
		for(int i=0; i<6; ++i)
		{
			PMXFace *f=new PMXFace();
			f->points[0]=i; f->points[1]=i+1; f->points[2]=i+2;
			m.faces.push_back(f);
		}
		m.face_continuing_datasets=m.faces.size();

		m.texture_continuing_datasets=1;
		m.texturePaths=new std::string[1+11];
		m.texturePaths[0]="tex.png";

		for(int i=0; i<2; ++i)
		{
			PMXMaterial *mat=new PMXMaterial();
			mat->name="mat";
			mat->diffuse=glm::vec4(1.0f);
			mat->textureIndex=0;
			mat->sphereIndex=-1;
			mat->toonTextureIndex=-1;
			mat->shareToon=1;
			mat->hasFaceNum=9;
			m.materials.push_back(mat);
		}
		m.material_continuing_datasets=m.materials.size();

		//morphs: 0 あ vertex, 1 UV, 2 extra UV1, 3 bone, 4 material (one), 5 material (all), 6 group
		PMXMorph *mo=morph(m, u8"あ", MORPH_TYPE_VERTEX);
		for(int i=0; i<4; ++i)
		{
			PMXVertexMorph *d=new PMXVertexMorph();
			d->vertexIndex=i;
			d->coordinateOffset=glm::vec3(0.1f*i, 0, 0);
			mo->offsetData.push_back(d);
		}
		mo=morph(m, "uv", MORPH_TYPE_UV);
		{
			PMXUVMorph *d=new PMXUVMorph(); d->vertexIndex=1; d->UVOffsetAmount=glm::vec4(0.1f); mo->offsetData.push_back(d);
		}
		mo=morph(m, "uv1", MORPH_TYPE_EXTRA_UV1);
		{
			PMXUVMorph *d=new PMXUVMorph(); d->vertexIndex=2; d->UVOffsetAmount=glm::vec4(0.1f); mo->offsetData.push_back(d);
		}
		mo=morph(m, "bone", MORPH_TYPE_BONE);
		{
			PMXBoneMorph *d=new PMXBoneMorph(); d->boneIndex=1; d->inertia=glm::vec3(0, 1, 0);
			d->rotationAmount=glm::vec4(0, 0.38f, 0, 0.92f); mo->offsetData.push_back(d);
		}
		for(int idx : {0, -1})
		{
			mo=morph(m, idx==0 ? "mat" : "matall", MORPH_TYPE_MATERIAL);
			PMXMaterialMorph *d=new PMXMaterialMorph();
			d->materialIndex=idx;
			d->offsetCalculationFormula=(idx==0) ? 0 : 1;
			d->diffuse=glm::vec4(0.5f);
			mo->offsetData.push_back(d);
		}
		mo=morph(m, "group", MORPH_TYPE_GROUP);
		for(int t : {0, 3, 4})
		{
			PMXGroupMorph *d=new PMXGroupMorph(); d->morphIndex=t; d->morphRate=0.5f; mo->offsetData.push_back(d);
		}
		for(PMXMorph *x : m.morphs) x->morphOffsetNum=x->offsetData.size();
		m.morph_continuing_datasets=m.morphs.size();
		m.display_frame_continuing_datasets=0;

		rigidBody(m, 1, RIGID_SHAPE_CAPSULE, RIGID_TYPE_FOLLOWER, glm::vec3(0, 12, 0));
		rigidBody(m, 7, RIGID_SHAPE_SPHERE, RIGID_TYPE_PHYSICS, glm::vec3(0, 14, -1));
		rigidBody(m, 8, RIGID_SHAPE_CUBE, RIGID_TYPE_PHYSICS_ROT_ONLY, glm::vec3(0, 15, 0));
		rigidBody(m, -1, RIGID_SHAPE_SPHERE, RIGID_TYPE_PHYSICS, glm::vec3(0, 20, 0));
		m.rigid_body_continuing_datasets=m.rigidBodies.size();

		for(int b : {1, -1})
		{
			PMXJoint *j=new PMXJoint();
			j->name="joint";
			j->type=0;
			j->relatedRigidBodyIndexA=(b==1) ? 0 : 3;
			j->relatedRigidBodyIndexB=(b==1) ? 1 : -1;
			j->position=glm::vec3(0, 13, -1);
			j->rotationLowerLimit=glm::vec3(-0.5f);
			j->rotationUpperLimit=glm::vec3(0.5f);
			j->springRotationConstant=glm::vec3(10.0f);
			m.joints.push_back(j);
		}
		m.joint_continuing_datasets=m.joints.size();
		return pm;
	}

	VMDInfo *makeTestMotion(const PMXInfo &model)
	{
		VMDInfo *v=new VMDInfo();
		//keys for the first bones / morphs (enough to cover every feature
		//without making big fuzzed models slow)
		const int maxBones=64, maxMorphs=64;
		for(int i=0; i<model.bone_continuing_datasets && i<maxBones; ++i)
		{
			for(unsigned frame : {0u, 4u})
			{
				VMDBoneFrame f;
				f.name=model.bones[i]->name;
				f.frame=frame+(unsigned)(i%3);
				f.translation=glm::vec3(0.0f, frame*0.1f, 0.0f);
				f.rotation=glm::normalize(glm::quat(1.0f, 0.1f*frame, 0.05f*i, 0.0f));
				BezierParameters bz;
				bz.X1=bz.Y1=bz.Z1=bz.R1=glm::vec2(20, 20);
				bz.X2=bz.Y2=bz.Z2=bz.R2=glm::vec2(107, 107);
				f.bezier=bz;
				v->boneFrames.push_back(f);
			}
		}
		for(int i=0; i<model.morph_continuing_datasets && i<maxMorphs; ++i)
		{
			for(unsigned frame : {0u, 3u})
			{
				VMDMorphFrame f;
				f.name=model.morphs[i]->name;
				f.frame=frame;
				f.value=frame ? 1.0f : 0.0f;
				v->morphFrames.push_back(f);
			}
		}
		VMDShowIKFrame ik;
		ik.frame=2;
		ik.show=true;
		for(int i=0; i<model.bone_continuing_datasets && i<maxBones; ++i)
		{
			if(model.bones[i]->IK) ik.IKList.push_back(VMDIKInfo{model.bones[i]->name, (i%2)==0});
		}
		v->showIKFrames.push_back(ik);
		return v;
	}

	//What the viewer relies on after loading (index buffer, texture and
	//draw ranges are only used on the GPU, where the dummy GL cannot see
	//them): abort if the loader let an invalid index through.
	static void check(bool ok, const char *what)
	{
		if(!ok) { fprintf(stderr, "invalid model after loading: %s\n", what); abort(); }
	}

	void checkModelInvariants(const PMXInfo &m)
	{
		const int nv=m.vertex_continuing_datasets, nb=m.bone_continuing_datasets, nt=m.texture_continuing_datasets;
		const int nm=m.material_continuing_datasets, nmo=m.morph_continuing_datasets, nr=m.rigid_body_continuing_datasets;
		check(nv==(int)m.vertices.size() && m.face_continuing_datasets==(int)m.faces.size() && nm==(int)m.materials.size() &&
		      nb==(int)m.bones.size() && nmo==(int)m.morphs.size() && nr==(int)m.rigidBodies.size() &&
		      m.joint_continuing_datasets==(int)m.joints.size() && m.display_frame_continuing_datasets==(int)m.displayFrames.size(),
		      "counts");
		check(nt>=0 && (nt==0 || m.texturePaths), "textures");
		check(m.extraUVCount<=4, "extra UV count");
		auto bone=[&](int b){ return b>=0 && b<nb; };
		for(const PMXVertex *v : m.vertices)
		{
			check(v->weight_transform_formula<=WEIGHT_FORMULA_SDEF, "weight type");
			for(int b : {v->boneIndex1, v->boneIndex2, v->boneIndex3, v->boneIndex4}) check(b==0 || bone(b), "vertex bone");
		}
		for(const PMXFace *f : m.faces) for(unsigned p : f->points) check(p<(unsigned)nv, "face vertex");
		long long indices=0;
		for(const PMXMaterial *mat : m.materials)
		{
			for(int t : {mat->textureIndex, mat->sphereIndex, mat->toonTextureIndex}) check(t>=-1 && t<nt, "material texture");
			check(mat->hasFaceNum>=0, "material face count");
			check(mat->shareToon<=1 && (mat->shareToon==0 || mat->shareToonTexture<=9), "material toon");
			indices+=mat->hasFaceNum;
		}
		check(indices<=(long long)m.faces.size()*3, "material face ranges");
		for(int i=0; i<nb; ++i)
		{
			const PMXBone *b=m.bones[i];
			check(b->parentBoneIndex==-1 ? !b->parent : (bone(b->parentBoneIndex) && b->parent==m.bones[b->parentBoneIndex]), "bone parent");
			if(b->giveRotation || b->giveTranslation) check(bone(b->givenParentBoneIndex), "inheritance bone");
			if(b->IK)
			{
				check(bone(b->IKTargetBoneIndex) && b->IKLinkNum==b->IKLinks.size() && b->IKLoopCount<=255, "IK");
				for(const PMXIKLink *l : b->IKLinks) check(bone(l->linkBoneIndex), "IK link");
			}
		}
		for(int i=0; i<nmo; ++i)
		{
			const PMXMorph *mo=m.morphs[i];
			check(mo->type<=MORPH_TYPE_MATERIAL && mo->morphOffsetNum==(int)mo->offsetData.size(), "morph");
			for(const PMXMorphData *d : mo->offsetData)
			{
				switch(mo->type)
				{
					case MORPH_TYPE_GROUP: { int t=((const PMXGroupMorph*)d)->morphIndex; check(t>=0 && t<nmo && t!=i, "group morph"); break; }
					case MORPH_TYPE_VERTEX: check(((const PMXVertexMorph*)d)->vertexIndex>=0 && ((const PMXVertexMorph*)d)->vertexIndex<nv, "vertex morph"); break;
					case MORPH_TYPE_BONE: check(bone(((const PMXBoneMorph*)d)->boneIndex), "bone morph"); break;
					case MORPH_TYPE_MATERIAL: { int t=((const PMXMaterialMorph*)d)->materialIndex; check(t>=-1 && t<nm, "material morph"); break; }
					default: check(((const PMXUVMorph*)d)->vertexIndex>=0 && ((const PMXUVMorph*)d)->vertexIndex<nv, "UV morph"); break;
				}
			}
		}
		for(const PMXDisplayFrame *f : m.displayFrames)
			for(const PMXDisplayFrameElement *e : f->elements) check(e->index<(unsigned)(e->target ? nmo : nb), "display frame");
		for(const PMXRigidBody *rb : m.rigidBodies)
			check((rb->relatedBoneIndex==-1 || bone(rb->relatedBoneIndex)) && rb->group<=15 && rb->shape<=RIGID_SHAPE_CAPSULE, "rigid body");
		for(const PMXJoint *j : m.joints)
			check(j->relatedRigidBodyIndexA>=0 && j->relatedRigidBodyIndexA<nr &&
			      j->relatedRigidBodyIndexB>=-1 && j->relatedRigidBodyIndexB<nr, "joint");
	}

	void exerciseModel(PMXInfo &model, VMDInfo &motion)
	{
		checkModelInvariants(model);
		VMDMotionController mc(model, motion, 0);
		mc.lastKeyFrame();
		for(int t=0; t<10; ++t)
		{
			mc.updateVertexMorphs();
			mc.updateBoneAnimation();
			mc.updateBoneMatrix();
			mc.isModelVisible();
			mc.advanceTime();
		}
		mc.setTime(3);
		mc.updateVertexMorphs();
		mc.updateBoneAnimation();

		BulletPhysics bullet("", "");
		MMDPhysics physics(model, &mc, &bullet);
		physics.resetToCurrentPose(2);
		for(int t=0; t<3; ++t)
		{
			mc.updateBoneAnimation();
			physics.updateBones(true);
			mc.updateBoneMatrix();
			mc.advanceTime();
		}
	}

	void exerciseSceneTracks(const VMDInfo &motion)
	{
		VMDCameraController camera(motion.cameraFrames);
		VMDLightController light(motion.lightFrames);
		unsigned last=std::max(camera.lastFrame(), light.lastFrame());
		for(float f : {0.0f, 0.5f, 1.0f, 2.5f, (float)last*0.5f, (float)last, (float)last+1.0f})
		{
			MMDCameraState c=camera.evaluate(f);
			c.viewMatrix();
			light.evaluate(f);
		}
	}
}

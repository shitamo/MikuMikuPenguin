//Animation tests without OpenGL (VMDMotionController with shaderProgram 0):
//Bezier curves, bone / morph interpolation, seeking (setTime) against normal
//playback, IK, inheritance (付与) and the deformation order, camera and light
//interpolation.
#include "mmp_test.h"

#include "pmx.h"
#include "vmd.h"
#include "motioncontroller.h"
#include "cameracontroller.h"
#include "lightcontroller.h"
#include "interpolation.h"

#include <cstring>

using namespace ClosedMMDFormat;

//bone indices of the test model (see PMX_BONES in test/gen_testdata.py)
enum { CENTER=0, BODY=1, ARM=2, ELBOW=3, WRIST=4, ARM_IK=5, LEG=6, HELPER_IK=7, HELPER_ROOT=8,
       HELPER_TIP=9, LEG_D=10, HALF_GIVE=11, SWING=12, MOVE_GIVE=14 };
//morph indices
enum { M_UP=0, M_COLOR=1, M_DARK=2, M_BONE=3, M_UV=4, M_GROUP=5 };

//Reference cubic Bezier from (0,0) to (1,1) with control points p1, p2 in
//0..127, computed independently of libmmp (bisection in double precision).
static float refBezier(float x, float x1, float y1, float x2, float y2)
{
	x1/=127.0; y1/=127.0; x2/=127.0; y2/=127.0;
	auto curve=[](double t, double p1, double p2) { return 3*(1-t)*(1-t)*t*p1 + 3*(1-t)*t*t*p2 + t*t*t; };
	double lo=0.0, hi=1.0;
	for(int i=0; i<100; ++i)
	{
		double mid=(lo+hi)/2;
		if(curve(mid, x1, x2)<x) lo=mid; else hi=mid;
	}
	return (float)curve((lo+hi)/2, y1, y2);
}

struct Scene
{
	PMXInfo *pmx;
	VMDInfo *vmd;
	VMDMotionController *ctrl;
	Scene()
	{
		mmptest::Quiet quiet;
		pmx=&readPMX(mmptest::dataDir()+"/", "test_model.pmx");
		vmd=&readVMD(mmptest::data("test_motion.vmd"));
		ctrl=new VMDMotionController(*pmx, *vmd, 0); //no OpenGL
	}
	~Scene() { delete ctrl; }
	glm::mat4 global(int bone) const { return pmx->bones[bone]->calculateGlobalMatrix(); }
	glm::vec3 position(int bone) const { return glm::vec3(global(bone)[3]); }
	glm::quat rotation(int bone) const { return glm::normalize(glm::quat_cast(glm::mat3(global(bone)))); }
	//jump to time t (1/60 s steps) as the viewer does when seeking
	void seek(int t)
	{
		ctrl->setTime(t);
		ctrl->updateVertexMorphs();
		ctrl->updateBoneAnimation();
		ctrl->updateBoneMatrix();
	}
	//one 1/60 s step of normal playback (Viewer::stepLogic without physics)
	void step()
	{
		if(!ctrl->advanceTime())
		{
			ctrl->updateVertexMorphs();
			ctrl->updateBoneAnimation();
		}
		ctrl->updateBoneMatrix();
	}
};

//Everything that makes up the pose at one time
struct Pose
{
	std::vector<glm::mat4> bones;
	std::vector<float> morphs;
	std::vector<glm::vec4> vertices;
	std::vector<glm::vec4> diffuse;
	static Pose of(const Scene &s)
	{
		Pose p;
		for(int i=0; i<s.pmx->bone_continuing_datasets; ++i) p.bones.push_back(s.global(i));
		p.morphs=s.ctrl->morphWeights;
		for(int i=0; i<s.pmx->vertex_continuing_datasets; ++i) p.vertices.push_back(s.ctrl->vertexData[i].position);
		for(const auto &m : s.ctrl->materialStates) p.diffuse.push_back(m.diffuse);
		return p;
	}
	//largest difference (0 if equal)
	float compare(const Pose &o, std::string &where) const
	{
		float worst=0.0f;
		auto note=[&](float d, const std::string &w) { if(d>worst) { worst=d; where=w; } };
		for(size_t i=0; i<bones.size(); ++i) note(mmptest::distance(bones[i], o.bones[i]), "bone "+mmptest::str(i));
		for(size_t i=0; i<morphs.size(); ++i) note(std::fabs(morphs[i]-o.morphs[i]), "morph "+mmptest::str(i));
		for(size_t i=0; i<vertices.size(); ++i) note(glm::length(vertices[i]-o.vertices[i]), "vertex "+mmptest::str(i));
		for(size_t i=0; i<diffuse.size(); ++i) note(glm::length(diffuse[i]-o.diffuse[i]), "material "+mmptest::str(i));
		return worst;
	}
};

static const int END_TIME=120; //last key: frame 60

// ---------------------------------------------------------------- Bezier

TEST(bezier_curve)
{
	//endpoints are exact for every curve
	const float curves[][4]={{20,20,107,107}, {64,0,64,127}, {0,64,64,127}, {127,0,0,127}, {0,0,127,127}};
	for(const auto &c : curves)
	{
		CHECK_NEAR(bezier(0.0f, c[0], c[1], c[2], c[3]), 0.0f, 1e-6f);
		CHECK_NEAR(bezier(1.0f, c[0], c[1], c[2], c[3]), 1.0f, 1e-3f);
		float last=-1.0f;
		bool monotonic=true;
		for(int i=0; i<=100; ++i)
		{
			float x=i/100.0f;
			float y=bezier(x, c[0], c[1], c[2], c[3]);
			CHECK_NEAR(y, refBezier(x, c[0], c[1], c[2], c[3]), 2e-3f);
			monotonic = monotonic && y>=last-1e-4f;
			last=y;
		}
		CHECK(monotonic);
	}
	//the default (linear) curve is the identity
	for(float x : {0.1f, 0.25f, 0.5f, 0.9f}) CHECK_NEAR(bezier(x, 20, 20, 107, 107), x, 2e-3f);
	//ease in/out: slow at both ends
	CHECK(bezier(0.1f, 64, 0, 64, 127) < 0.05f);
	CHECK(bezier(0.9f, 64, 0, 64, 127) > 0.95f);
}

// ---------------------------------------------------------------- bones

TEST(bone_rest_pose_at_start)
{
	Scene s;
	s.seek(0);
	//every first key is the rest pose: all bones where the model puts them
	for(int i=0; i<s.pmx->bone_continuing_datasets; ++i)
	{
		CHECK_NEAR(s.position(i), s.pmx->bones[i]->position, 1e-4f);
		CHECK_NEAR(s.rotation(i), glm::quat(1, 0, 0, 0), 1e-5f);
	}
	CHECK_EQ(s.ctrl->lastKeyFrame(), 60u);
}

TEST(bone_bezier_interpolation)
{
	Scene s;
	//センター: X eases in and out from 0 to 4 between frames 0 and 30
	s.seek(30); //frame 15
	float expected=4.0f*refBezier(0.5f, 64, 0, 64, 127);
	CHECK_NEAR(s.position(CENTER), glm::vec3(expected, 0.0f, 0.0f), 2e-3f);
	s.seek(15); //frame 7.5
	CHECK_NEAR(s.position(CENTER).x, 4.0f*refBezier(0.25f, 64, 0, 64, 127), 2e-3f);
	s.seek(60); //frame 30: on the key
	CHECK_NEAR(s.position(CENTER), glm::vec3(4.0f, 0.0f, 0.0f), 1e-4f);
	//frames 30-60 use the curve of the key at 60 (ease out)
	s.seek(90);
	CHECK_NEAR(s.position(CENTER).x, 4.0f*(1.0f-refBezier(0.5f, 0, 64, 64, 127)), 2e-3f);

	//体: rotation about Y with an ease in/out curve (keys 0 and 40)
	s.seek(40); //frame 20
	glm::quat target=glm::angleAxis(glm::radians(-60.0f), glm::vec3(0, 1, 0));
	glm::quat q=glm::slerp(glm::quat(1, 0, 0, 0), target, refBezier(0.5f, 64, 0, 64, 127));
	CHECK_NEAR(s.rotation(BODY), q, 1e-4f);
	s.seek(80); //frame 40
	CHECK_NEAR(s.rotation(BODY), target, 1e-5f);
	//past the last key the last pose is held
	s.seek(200);
	CHECK_NEAR(s.rotation(BODY), target, 1e-5f);
	CHECK_NEAR(s.position(CENTER), glm::vec3(0.0f), 1e-4f);
}

TEST(seek_matches_playback)
{
	//pose of every time step during normal playback
	Scene play;
	play.seek(0);
	std::vector<Pose> poses;
	poses.push_back(Pose::of(play));
	for(int t=1; t<=END_TIME+20; ++t)
	{
		play.step();
		CHECK_EQ(play.ctrl->getTime(), t);
		poses.push_back(Pose::of(play));
	}

	//seeking forwards, backwards and at random must give the same poses
	Scene seek;
	std::vector<int> order;
	for(int t=0; t<(int)poses.size(); ++t) order.push_back(t);
	for(int t=(int)poses.size()-1; t>=0; --t) order.push_back(t);
	unsigned r=12345;
	for(int i=0; i<200; ++i) { r=r*1103515245u+12345u; order.push_back((r>>8)%poses.size()); }
	int bad=0;
	for(int t : order)
	{
		seek.seek(t);
		std::string where;
		float d=Pose::of(seek).compare(poses[t], where);
		if(d>1e-4f)
		{
			if(++bad<=5) mmptest::fail(__FILE__, __LINE__, "seek to time "+mmptest::str(t)+" differs from playback at "+where+" by "+mmptest::str(d));
		}
	}
	++mmptest::checks();
	CHECK_EQ(bad, 0);
}

// ---------------------------------------------------------------- morphs

TEST(morph_interpolation)
{
	Scene s;
	s.seek(30); //frame 15
	const std::vector<float> &w=s.ctrl->morphWeights;
	CHECK_NEAR(w[M_UP], 0.5f, 1e-5f);     //0 -> 1 over frames 0-30
	CHECK_NEAR(w[M_COLOR], 0.75f, 1e-5f); //0 -> 1 over frames 0-20
	CHECK_NEAR(w[M_BONE], 0.25f, 1e-5f);  //0 -> 1 over frames 0-60
	CHECK_NEAR(w[M_GROUP], 0.0f, 1e-6f);  //before its first key
	CHECK_NEAR(w[M_UV], 0.0f, 1e-6f);     //no keys
	//vertex morph: the top of the 体 box moves up by 2*w
	CHECK_NEAR(s.ctrl->vertexData[16].position.y, 6.0f+2.0f*0.5f, 1e-5f);
	CHECK_NEAR(s.ctrl->vertexData[0].position.y, 2.0f, 1e-6f); //bottom: not in the morph

	s.seek(90); //frame 45: グループ 0.5 adds 0.25 to 上 and 0.5 to 暗く
	CHECK_NEAR(w[M_GROUP], 0.5f, 1e-5f);
	CHECK_NEAR(w[M_UP], 0.5f+0.25f, 1e-5f);
	CHECK_NEAR(w[M_DARK], 0.5f, 1e-5f);
	CHECK_NEAR(s.ctrl->vertexData[16].position.y, 6.0f+2.0f*0.75f, 1e-5f);
	//material morphs: base * mix(1, mul, w) + add * w
	CHECK_NEAR(s.ctrl->materialStates[0].diffuse, glm::vec4(0.9f*0.75f, 0.2f*0.75f, 0.2f*0.75f+1.0f, 1.0f), 1e-5f);
	CHECK_NEAR(s.ctrl->materialStates[1].diffuse, glm::vec4(0.2f*0.75f, 0.8f*0.75f, 0.3f*0.75f, 1.0f), 1e-5f);
}

TEST(bone_morph)
{
	Scene s;
	s.seek(60); //frame 30: 骨 = 0.5 moves 体 by (0.5, 0, 0) on top of センター's (4, 0, 0)
	CHECK_NEAR(s.ctrl->morphWeights[M_BONE], 0.5f, 1e-5f);
	CHECK_NEAR(s.position(BODY), glm::vec3(4.5f, 2.0f, 0.0f), 1e-4f);
}

// ---------------------------------------------------------------- IK and inheritance

TEST(ik_reaches_target)
{
	Scene s;
	for(int t : {30, 60, 90})
	{
		s.seek(t);
		CHECK(s.ctrl->isIKEnabled(ARM_IK));
		CHECK_NEAR(s.position(WRIST), s.position(ARM_IK), 0.05f);
		//the chain keeps its length
		CHECK_NEAR(glm::length(s.position(ELBOW)-s.position(ARM)), 4.0f, 1e-3f);
		CHECK_NEAR(glm::length(s.position(WRIST)-s.position(ELBOW)), 4.0f, 1e-3f);
	}
	s.seek(60);
	glm::vec3 ikPos=s.position(ARM_IK);
	CHECK_NEAR(ikPos, glm::vec3(4.0f+3.0f, 12.0f, 0.0f), 1e-3f); //センター + key
	//the elbow did bend: two segments of 4 reaching from 腕 (moved by the
	//bone morph 骨 to (4.5,6,0)) to the target bend by acos(d^2/32 - 1)
	CHECK_NEAR(s.position(ARM), glm::vec3(4.5f, 6.0f, 0.0f), 1e-4f);
	float d=glm::length(ikPos-s.position(ARM));
	float bend=glm::degrees(acosf(d*d/32.0f-1.0f));
	glm::vec3 upper=glm::normalize(s.position(ELBOW)-s.position(ARM));
	glm::vec3 lower=glm::normalize(s.position(WRIST)-s.position(ELBOW));
	CHECK_NEAR(glm::degrees(acosf(glm::dot(upper, lower))), bend, 1.0f);
	CHECK(bend>60.0f);
}

TEST(ik_switched_off_by_vmd)
{
	Scene s;
	s.seek(98); //frame 49
	CHECK(s.ctrl->isIKEnabled(ARM_IK));
	s.seek(100); //frame 50: the show/IK key turns 腕IK off
	CHECK(!s.ctrl->isIKEnabled(ARM_IK));
	CHECK(s.ctrl->isModelVisible());
	s.seek(110);
	//without IK the arm keeps the 体 pose: straight up from 腕
	glm::vec3 arm=s.position(ARM), wrist=s.position(WRIST);
	CHECK_NEAR(wrist-arm, glm::vec3(0.0f, 8.0f, 0.0f), 1e-3f);
	CHECK(glm::length(wrist-s.position(ARM_IK)) > 1.0f);
}

TEST(rotation_and_translation_inheritance)
{
	Scene s;
	for(int t : {20, 40, 60, 75, 100})
	{
		s.seek(t);
		//足D: rate 1 of 足
		CHECK_NEAR(s.rotation(LEG_D), s.rotation(LEG), 1e-5f);
		//半付与: half of 体's rotation
		glm::quat body=glm::normalize(glm::quat_cast(glm::mat3(s.pmx->bones[BODY]->Local)));
		CHECK_NEAR(s.rotation(HALF_GIVE), glm::slerp(glm::quat(1, 0, 0, 0), body, 0.5f), 1e-4f);
		//移動付与: half of センター's translation (on top of following センター)
		glm::vec3 center=s.position(CENTER);
		CHECK_NEAR(s.position(MOVE_GIVE), glm::vec3(-3.0f, 2.0f, -1.0f)+1.5f*center, 1e-4f);
	}
	s.seek(80);
	glm::quat full=glm::angleAxis(glm::radians(-60.0f), glm::vec3(0, 1, 0));
	CHECK_NEAR(s.rotation(HALF_GIVE), glm::angleAxis(glm::radians(-30.0f), glm::vec3(0, 1, 0)), 1e-4f);
	CHECK_NEAR(s.rotation(BODY), full, 1e-5f);
}

TEST(deformation_order_ik_after_inheritance)
{
	//補助IK (index 7, level 2) hangs under 足D (index 10, level 1), which
	//inherits 足's rotation.  足D must be deformed before 補助IK solves its
	//chain, so 補助先 follows 補助IK wherever 足 turns it.
	Scene s;
	for(int t : {30, 60, 90, 120})
	{
		s.seek(t);
		glm::vec3 goal=s.position(HELPER_IK);
		CHECK(glm::length(goal-glm::vec3(8.0f, 4.0f, 0.0f)-glm::vec3(s.position(CENTER).x, 0, 0)) > 1.0f); //moved by 足
		CHECK_NEAR(s.position(HELPER_TIP), goal, 0.05f);
		//補助根 ends up with 足's rotation
		CHECK_NEAR(s.rotation(HELPER_ROOT), s.rotation(LEG), 1e-3f);
	}
}

// ---------------------------------------------------------------- camera and light

static VMDCameraController loadCamera()
{
	mmptest::Quiet quiet;
	return VMDCameraController(readVMD(mmptest::data("test_camera.vmd")).cameraFrames);
}

TEST(camera_interpolation)
{
	VMDCameraController cam=loadCamera();
	CHECK(!cam.empty());
	CHECK_EQ(cam.lastFrame(), 60u);

	MMDCameraState s=cam.evaluate(0.0f); //on the first key
	CHECK_NEAR(s.interest, glm::vec3(0.0f, 8.0f, 0.0f), 1e-4f);
	CHECK_NEAR(s.distance, -40.0f, 1e-4f);
	CHECK_NEAR(s.rotation, glm::vec3(0.1f, 0.0f, 0.0f), 1e-5f);
	CHECK_NEAR(s.fov, glm::radians(30.0f), 1e-5f);
	CHECK(s.perspective);

	//frames 0-30: the key at 30 has ease in/out for X, ease out for the rotation
	s=cam.evaluate(15.0f);
	CHECK_NEAR(s.interest.x, 2.0f*refBezier(0.5f, 64, 0, 64, 127), 2e-3f);
	CHECK_NEAR(s.interest.y, 8.0f, 1e-4f);
	float r=refBezier(0.5f, 0, 64, 64, 127);
	CHECK_NEAR(s.rotation, glm::mix(glm::vec3(0.1f, 0.0f, 0.0f), glm::vec3(0.2f, 0.5f, 0.0f), r), 1e-3f);
	CHECK_NEAR(s.distance, -35.0f, 1e-3f);
	CHECK_NEAR(s.fov, glm::radians(35.0f), 1e-4f);

	//keys one frame apart are a cut: no interpolation between 30 and 31
	s=cam.evaluate(30.5f);
	CHECK_NEAR(s.distance, -30.0f, 1e-4f);
	s=cam.evaluate(31.0f);
	CHECK_NEAR(s.distance, -50.0f, 1e-4f);
	CHECK_NEAR(s.interest, glm::vec3(0.0f, 6.0f, 0.0f), 1e-4f);
	//linear between 31 and 60
	s=cam.evaluate(45.5f);
	CHECK_NEAR(s.distance, -47.5f, 1e-3f);
	CHECK_NEAR(s.interest.y, 6.5f, 1e-3f);
	//held after the last key
	s=cam.evaluate(500.0f);
	CHECK_NEAR(s.distance, -45.0f, 1e-4f);
}

TEST(camera_view)
{
	VMDCameraFrame k=VMDCameraFrame();
	k.frame=0;
	k.distance=-45.0f;
	k.position=glm::vec3(0.0f, 10.0f, 0.0f);
	k.viewAngle=30;
	k.perspective=false;
	for(int i=0; i<24; ++i) k.interpolationParameters[i]=(i%4<2) ? 20 : 107;
	VMDCameraController cam(std::vector<VMDCameraFrame>(1, k));
	MMDCameraState s=cam.evaluate(10.0f);
	//MMD's front view: the camera is |distance| in front of the interest point
	CHECK_NEAR(s.eye, glm::vec3(0.0f, 10.0f, 45.0f), 1e-4f);
	CHECK_NEAR(glm::normalize(s.center-s.eye), glm::vec3(0.0f, 0.0f, -1.0f), 1e-5f);
	CHECK_NEAR(s.up, glm::vec3(0.0f, 1.0f, 0.0f), 1e-5f);
	glm::vec4 p=s.viewMatrix()*glm::vec4(0.0f, 10.0f, 0.0f, 1.0f);
	CHECK_NEAR(glm::vec3(p), glm::vec3(0.0f, 0.0f, -45.0f), 1e-3f);
	//Y rotation of 90 degrees looks from the side
	k.rotation=glm::vec3(0.0f, glm::radians(90.0f), 0.0f);
	k.perspective=true; //"perspective off" as stored
	s=VMDCameraController(std::vector<VMDCameraFrame>(1, k)).evaluate(0.0f);
	CHECK_NEAR(s.eye, glm::vec3(45.0f, 10.0f, 0.0f), 1e-3f);
	CHECK(!s.perspective);

	//no keys: MMD's default camera
	s=VMDCameraController().evaluate(0.0f);
	CHECK_NEAR(s.interest, glm::vec3(0.0f, 10.0f, 0.0f), 1e-6f);
	CHECK_NEAR(s.distance, -45.0f, 1e-6f);
}

TEST(light_interpolation)
{
	mmptest::Quiet *quiet=new mmptest::Quiet;
	VMDLightController light(readVMD(mmptest::data("test_camera.vmd")).lightFrames);
	delete quiet;
	CHECK_EQ(light.lastFrame(), 60u);
	MMDLightState s=light.evaluate(0.0f);
	CHECK_NEAR(s.color, glm::vec3(0.6f), 1e-5f);
	CHECK_NEAR(s.direction, glm::normalize(glm::vec3(-0.5f, -1.0f, -0.5f)), 1e-5f);
	s=light.evaluate(30.0f); //linear
	CHECK_NEAR(s.color, glm::vec3(0.8f, 0.7f, 0.6f), 1e-5f);
	CHECK_NEAR(s.direction, glm::normalize(glm::vec3(0.0f, -1.0f, -0.5f)), 1e-5f);
	s=light.evaluate(90.0f);
	CHECK_NEAR(s.color, glm::vec3(1.0f, 0.8f, 0.6f), 1e-5f);
	CHECK_NEAR(s.direction, glm::normalize(glm::vec3(0.5f, -1.0f, -0.5f)), 1e-5f);
	//no keys: MMD's default light
	s=VMDLightController().evaluate(10.0f);
	CHECK_NEAR(s.color, glm::vec3(154.0f/255.0f), 1e-6f);
	CHECK_NEAR(s.direction, VMDLightController::defaultLight().direction, 1e-6f);
}

TEST_MAIN()

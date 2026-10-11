#include "cameracontroller.h"
#include "interpolation.h"

#include <algorithm>

using namespace ClosedMMDFormat;

VMDCameraController::VMDCameraController(const std::vector<VMDCameraFrame> &frames)
	: keys(frames)
{
	std::stable_sort(keys.begin(), keys.end(),
		[](const VMDCameraFrame &a, const VMDCameraFrame &b) { return a.frame < b.frame; });
}

//Bezier progress for curve number `curve` (0:X 1:Y 2:Z 3:rotation 4:distance 5:fov)
static float curveProgress(const VMDCameraFrame &key, int curve, float t)
{
	const unsigned char *b = (const unsigned char*)key.interpolationParameters + curve*4;
	float x1=b[0], x2=b[1], y1=b[2], y2=b[3];
	if(x1==y1 && x2==y2) return t; //linear
	return bezier(t, x1, y1, x2, y2);
}

static MMDCameraState fromKey(const VMDCameraFrame &k)
{
	MMDCameraState s;
	s.interest = k.position;
	s.rotation = k.rotation;
	s.distance = k.distance;
	s.fov = glm::radians((float)k.viewAngle);
	s.perspective = !k.perspective;
	return s;
}

static void computeView(MMDCameraState &s)
{
	//Same construction as MMD-compatible players (e.g. saba): the camera sits
	//|distance| away along +Z, rotated by Ry * Rz(-Z axis) * Rx, and looks
	//back along its local -Z axis.
	glm::mat4 rot(1.0f);
	rot = glm::rotate(rot, s.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	rot = glm::rotate(rot, s.rotation.z, glm::vec3(0.0f, 0.0f, -1.0f));
	rot = glm::rotate(rot, s.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 view = rot * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, std::abs(s.distance)));
	
	s.eye    = glm::vec3(view[3]) + s.interest;
	s.center = glm::mat3(view) * glm::vec3(0.0f, 0.0f, -1.0f) + s.eye;
	s.up     = glm::mat3(view) * glm::vec3(0.0f, 1.0f, 0.0f);
}

MMDCameraState VMDCameraController::evaluate(float frame) const
{
	MMDCameraState s;
	if(keys.empty())
	{
		s.interest=glm::vec3(0.0f,10.0f,0.0f); s.rotation=glm::vec3(0.0f); s.distance=-45.0f;
		s.fov=glm::radians(30.0f); s.perspective=true;
		computeView(s);
		return s;
	}
	
	//first key after `frame`
	auto it = std::upper_bound(keys.begin(), keys.end(), frame,
		[](float f, const VMDCameraFrame &k) { return f < (float)k.frame; });
	
	if(it == keys.end()) s = fromKey(keys.back());
	else if(it == keys.begin()) s = fromKey(keys.front());
	else
	{
		const VMDCameraFrame &k1 = *it;
		const VMDCameraFrame &k0 = *(it-1);
		if(k1.frame - k0.frame <= 1) s = fromKey(k0); //camera cut
		else
		{
			float t = (frame - (float)k0.frame) / (float)(k1.frame - k0.frame);
			t = glm::clamp(t, 0.0f, 1.0f);
			MMDCameraState a = fromKey(k0), b = fromKey(k1);
			s = a;
			s.interest.x = glm::mix(a.interest.x, b.interest.x, curveProgress(k1, 0, t));
			s.interest.y = glm::mix(a.interest.y, b.interest.y, curveProgress(k1, 1, t));
			s.interest.z = glm::mix(a.interest.z, b.interest.z, curveProgress(k1, 2, t));
			s.rotation   = glm::mix(a.rotation, b.rotation, curveProgress(k1, 3, t));
			s.distance   = glm::mix(a.distance, b.distance, curveProgress(k1, 4, t));
			s.fov        = glm::mix(a.fov, b.fov, curveProgress(k1, 5, t));
		}
	}
	computeView(s);
	return s;
}

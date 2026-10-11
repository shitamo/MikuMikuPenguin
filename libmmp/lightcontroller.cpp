#include "lightcontroller.h"

#include <algorithm>

using namespace ClosedMMDFormat;

static glm::vec3 safeNormalize(const glm::vec3 &v)
{
	float l=glm::length(v);
	if(!(l>1.0e-6f)) return VMDLightController::defaultLight().direction;
	return v/l;
}

VMDLightController::VMDLightController(const std::vector<VMDLightFrame> &frames):keys(frames)
{
	std::stable_sort(keys.begin(), keys.end(),
		[](const VMDLightFrame &a, const VMDLightFrame &b){ return a.frame<b.frame; });
}

MMDLightState VMDLightController::defaultLight()
{
	MMDLightState s;
	s.color=glm::vec3(154.0f/255.0f);
	//(-0.5,-1.0,0.5) in MMD's left-handed coordinates (Z is negated here)
	s.direction=glm::normalize(glm::vec3(-0.5f,-1.0f,-0.5f));
	return s;
}

MMDLightState VMDLightController::evaluate(float frame) const
{
	if(keys.empty()) return defaultLight();

	MMDLightState s;
	auto it=std::upper_bound(keys.begin(), keys.end(), frame,
		[](float f, const VMDLightFrame &k){ return f<(float)k.frame; });
	if(it==keys.begin())
	{
		s.color=keys.front().color;
		s.direction=safeNormalize(keys.front().position);
	}
	else if(it==keys.end())
	{
		s.color=keys.back().color;
		s.direction=safeNormalize(keys.back().position);
	}
	else
	{
		const VMDLightFrame &k0=*(it-1), &k1=*it;
		float t=(frame-(float)k0.frame)/(float)(k1.frame-k0.frame);
		s.color=glm::mix(k0.color, k1.color, t);
		s.direction=safeNormalize(glm::mix(k0.position, k1.position, t));
	}
	s.color=glm::clamp(s.color, 0.0f, 1.0f);
	return s;
}

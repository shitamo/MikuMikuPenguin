#ifndef MMP_LIGHT_CONTROLLER_H
#define MMP_LIGHT_CONTROLLER_H

#include "mmp_glm.h"
#include "vmd.h"

#include <vector>

/*! Light state evaluated from VMD light keyframes (OpenGL coordinates). */
struct MMDLightState
{
	glm::vec3 color;      //!< light colour (0..1, MMD default 154/255)
	glm::vec3 direction;  //!< normalised direction the light travels
};

/*! Interpolates VMD light frames like MMD (linear between keys; before the
 *  first / after the last key the nearest key is held).  Without keys the
 *  MMD default light is returned. */
class VMDLightController
{
	public:
	VMDLightController() {}
	explicit VMDLightController(const std::vector<ClosedMMDFormat::VMDLightFrame> &frames);

	bool empty() const { return keys.empty(); }

	//! Evaluate the light at a (fractional) 30 fps VMD frame.
	MMDLightState evaluate(float frame) const;

	//! The MMD default light: colour (154,154,154), direction (-0.5,-1.0,0.5) in MMD coordinates.
	static MMDLightState defaultLight();

	private:
	std::vector<ClosedMMDFormat::VMDLightFrame> keys;
};

#endif

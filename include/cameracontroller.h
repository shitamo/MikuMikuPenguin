#ifndef MMP_CAMERA_CONTROLLER_H
#define MMP_CAMERA_CONTROLLER_H

#include "mmp_glm.h"
#include "vmd.h"

#include <vector>

/*! Camera pose evaluated from VMD camera keyframes (OpenGL coordinates). */
struct MMDCameraState
{
	glm::vec3 interest;   //!< point of interest
	glm::vec3 rotation;   //!< Euler angles (radians) as stored in the VMD
	float distance;       //!< distance from the point of interest (MMD: negative = in front)
	float fov;            //!< vertical field of view in radians
	bool perspective;     //!< false: orthographic-like ("perspective OFF" in MMD)

	glm::vec3 eye;        //!< camera position
	glm::vec3 center;     //!< point the camera looks at
	glm::vec3 up;         //!< up vector

	glm::mat4 viewMatrix() const { return glm::lookAt(eye, center, up); }
};

/*! Interpolates VMD camera frames like MMD (per-channel Bezier curves of the
 *  destination key; keys one frame apart are a cut). */
class VMDCameraController
{
	public:
	VMDCameraController() {}
	explicit VMDCameraController(const std::vector<ClosedMMDFormat::VMDCameraFrame> &frames);

	bool empty() const { return keys.empty(); }
	unsigned lastFrame() const { return keys.empty() ? 0 : keys.back().frame; }

	//! Evaluate the camera at a (fractional) 30 fps VMD frame.
	MMDCameraState evaluate(float frame) const;

	private:
	std::vector<ClosedMMDFormat::VMDCameraFrame> keys;
};

#endif

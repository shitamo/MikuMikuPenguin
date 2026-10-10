#ifndef MMP_GLM_H
#define MMP_GLM_H

/*
 * Common GLM configuration for libmmp.
 *
 * libmmp was written against GLM 0.9.4, where
 *  - matrices/quaternions were default-initialised to identity, and
 *  - the GTX extensions could be used without opting in.
 * Modern GLM (0.9.9 / 1.x) changed both of these, so restore the old
 * behaviour here.  All angles are in radians (GLM >= 0.9.6 semantics).
 */
#ifndef GLM_FORCE_CTOR_INIT
#define GLM_FORCE_CTOR_INIT
#endif
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace mmp {
/* GLM 0.9.4 provided normalize() for scalars (returned the sign, +1 for 0).
 * The viewer relied on that, so keep an equivalent helper. */
inline float scalarNormalize(float x) { return x < 0.0f ? -1.0f : 1.0f; }
}

#endif

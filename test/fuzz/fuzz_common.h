//Shared code of the libFuzzer targets (test/fuzz/fuzz_*.cpp) and of the
//regression test (test/regress_test.cpp): run a loaded model / motion
//through the parts of libmmp that do not need an OpenGL context.
#ifndef MMP_FUZZ_COMMON_H
#define MMP_FUZZ_COMMON_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "pmx.h"
#include "vmd.h"

namespace mmpfuzz
{
	//Replace the libepoxy entry points used by VMDMotionController with
	//no-op stubs, so it can run without a GL context.  Call once.
	void installDummyGL();

	//Send libmmp's progress output (std::cout / std::cerr) to nowhere.
	void silenceOutput();

	//Store data in an in-memory file and return a path for it
	//("/proc/self/fd/N"); the loaders only take file names.
	std::string memoryFile(const uint8_t *data, size_t size);

	//A small built-in model whose bone / morph names match the generated
	//VMD seeds (IK, inheritance, all morph types, physics).
	ClosedMMDFormat::PMXInfo *makeTestModel();

	//A motion with a few keys for every bone and morph of the model.
	ClosedMMDFormat::VMDInfo *makeTestMotion(const ClosedMMDFormat::PMXInfo &model);

	//Abort if a loaded model has an index the viewer cannot use safely
	//(face vertices, material ranges and textures, bones, morphs, ...).
	void checkModelInvariants(const ClosedMMDFormat::PMXInfo &model);

	//Animate the model with the motion (morphs, bones, IK, inheritance,
	//bone matrices) for a few frames, then build and step its physics.
	//Checks the invariants first.
	void exerciseModel(ClosedMMDFormat::PMXInfo &model, ClosedMMDFormat::VMDInfo &motion);

	//Camera and light controllers of a motion.
	void exerciseSceneTracks(const ClosedMMDFormat::VMDInfo &motion);
}

#endif

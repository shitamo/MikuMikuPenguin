//libFuzzer target: VMD loader (readVMD), then the motion played on a small
//built-in model (bone/morph keys, show/IK keys, physics) and the camera and
//light tracks.
#include "fuzz_common.h"

#include <memory>
#include <stdexcept>

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	mmpfuzz::installDummyGL();
	mmpfuzz::silenceOutput();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	std::string path=mmpfuzz::memoryFile(data, size);
	std::unique_ptr<ClosedMMDFormat::VMDInfo> motion;
	try { motion.reset(&ClosedMMDFormat::readVMD(path)); }
	catch(const std::runtime_error &) { return 0; } //rejected: fine
	mmpfuzz::exerciseSceneTracks(*motion);
	std::unique_ptr<ClosedMMDFormat::PMXInfo> model(mmpfuzz::makeTestModel());
	mmpfuzz::exerciseModel(*model, *motion);
	return 0;
}

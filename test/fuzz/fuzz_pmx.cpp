//libFuzzer target: PMX loader (readPMX, which also accepts PMD data), then
//animation, IK, morphs and physics of the loaded model.
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
	std::unique_ptr<ClosedMMDFormat::PMXInfo> model;
	try { model.reset(&ClosedMMDFormat::readPMX("", path)); }
	catch(const std::runtime_error &) { return 0; } //rejected: fine
	std::unique_ptr<ClosedMMDFormat::VMDInfo> motion(mmpfuzz::makeTestMotion(*model));
	mmpfuzz::exerciseModel(*model, *motion);
	return 0;
}

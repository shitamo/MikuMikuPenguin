//Regression test for the PMX / PMD / VMD loaders (files in test/regress/).
//
//  regress_test FILE...        load every file and play it like the fuzz
//                              targets (motion, IK, morphs, physics)
//  regress_test --deep-bone-chain
//                              a PMD model whose 65535 bones form one chain
//
//A file whose name contains ".reject." must be rejected with
//std::runtime_error; any other file must load.  Crashes, hangs and (in a
//sanitizer build) memory errors make the test fail.  -v shows libmmp's
//output.
#include "fuzz/fuzz_common.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ClosedMMDFormat;

static bool endsWith(const std::string &s, const char *suffix)
{
	size_t n=strlen(suffix);
	return s.size()>=n && s.compare(s.size()-n, n, suffix)==0;
}

//Load (and play) one file; throws std::runtime_error when it is rejected
static void run(const std::string &path)
{
	if(endsWith(path, ".vmd"))
	{
		std::unique_ptr<VMDInfo> motion(&readVMD(path));
		mmpfuzz::exerciseSceneTracks(*motion);
		std::unique_ptr<PMXInfo> model(mmpfuzz::makeTestModel());
		mmpfuzz::exerciseModel(*model, *motion);
	}
	else
	{
		std::unique_ptr<PMXInfo> model(endsWith(path, ".pmd") ? &readPMD("", path) : &readPMX("", path));
		std::unique_ptr<VMDInfo> motion(mmpfuzz::makeTestMotion(*model));
		mmpfuzz::exerciseModel(*model, *motion);
	}
}

static void put16(std::vector<uint8_t> &d, uint16_t v) { d.push_back(v&0xff); d.push_back(v>>8); }
static void put32(std::vector<uint8_t> &d, uint32_t v) { for(int i=0; i<4; ++i) d.push_back((v>>(8*i))&0xff); }
static void putF(std::vector<uint8_t> &d, float f) { uint32_t v; memcpy(&v, &f, 4); put32(d, v); }

//PMD with 65535 bones, each the child of the previous one: the recursive
//bone matrix calculation overflowed the stack and the parent cycle check
//took quadratic time.
static std::vector<uint8_t> deepBoneChainPMD()
{
	std::vector<uint8_t> d={'P','m','d'};
	putF(d, 1.0f);
	d.resize(d.size()+20+256, 0);
	put32(d, 1); //one vertex on the last bone
	for(int i=0; i<8; ++i) putF(d, 0.0f);
	put16(d, 65534); put16(d, 65534); d.push_back(100); d.push_back(0);
	put32(d, 0); //faces
	put32(d, 0); //materials
	put16(d, 65535);
	for(int i=0; i<65535; ++i)
	{
		char name[20]={0};
		snprintf(name, sizeof(name), "b%d", i);
		d.insert(d.end(), name, name+20);
		put16(d, i==0 ? 0xffff : i-1); //parent
		put16(d, 0); d.push_back(0); put16(d, 0);
		putF(d, 0.0f); putF(d, i*0.001f); putF(d, 0.0f);
	}
	put16(d, 0); //IK
	put16(d, 0); //skins
	d.push_back(0); d.push_back(0); put32(d, 0); //display lists
	return d;
}

int main(int argc, char **argv)
{
	mmpfuzz::installDummyGL();
	bool verbose=false;
	std::vector<std::string> files;
	for(int i=1; i<argc; ++i)
	{
		if(strcmp(argv[i], "-v")==0) verbose=true;
		else files.push_back(argv[i]);
	}
	std::streambuf *out=std::cout.rdbuf(), *err=std::cerr.rdbuf();
	if(!verbose) mmpfuzz::silenceOutput();
	auto report=[&](const std::string &line) { std::ostream o(out); o<<line<<std::endl; };

	int failures=0;
	for(const std::string &arg : files)
	{
		std::string path=arg;
		if(arg=="--deep-bone-chain")
		{
			std::vector<uint8_t> generated=deepBoneChainPMD();
			path=mmpfuzz::memoryFile(generated.data(), generated.size());
		}
		bool mustReject=arg.find(".reject.")!=std::string::npos;
		std::string result;
		bool rejected=false;
		try
		{
			if(arg=="--deep-bone-chain")
			{
				std::unique_ptr<PMXInfo> model(&readPMD("", path));
				std::unique_ptr<VMDInfo> motion(mmpfuzz::makeTestMotion(*model));
				mmpfuzz::exerciseModel(*model, *motion);
			}
			else run(path);
		}
		catch(const std::runtime_error &e)
		{
			rejected=true;
			result=e.what();
		}
		bool ok=(rejected==mustReject);
		if(!ok) ++failures;
		report(std::string(ok ? "PASS " : "FAIL ")+arg+": "+(rejected ? "rejected ("+result+")" : "loaded")
		       +(ok ? "" : mustReject ? " - expected a load error" : " - expected to load"));
	}
	std::cout.rdbuf(out);
	std::cerr.rdbuf(err);
	return failures ? 1 : 0;
}

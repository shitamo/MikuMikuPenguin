//Tiny test helpers for the libmmp unit tests (no external framework).
//
//Every test program takes the directory with the generated test data
//(test/gen_testdata.py) as its first argument.  CHECK* report the failing
//line and keep going; main() returns non-zero if anything failed.
#ifndef MMP_TEST_H
#define MMP_TEST_H

#include "mmp_glm.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

namespace mmptest
{
	inline int &failures() { static int n=0; return n; }
	inline int &checks() { static int n=0; return n; }
	inline std::string &dataDir() { static std::string d; return d; }
	inline std::string data(const std::string &name) { return dataDir()+"/"+name; }

	inline void fail(const char *file, int line, const std::string &what)
	{
		++failures();
		std::fprintf(stderr, "%s:%d: FAILED: %s\n", file, line, what.c_str());
	}

	template<class T> std::string str(const T &v) { std::ostringstream s; s<<v; return s.str(); }
	inline std::string str(const glm::vec2 &v) { std::ostringstream s; s<<"("<<v.x<<", "<<v.y<<")"; return s.str(); }
	inline std::string str(const glm::vec3 &v) { std::ostringstream s; s<<"("<<v.x<<", "<<v.y<<", "<<v.z<<")"; return s.str(); }
	inline std::string str(const glm::vec4 &v) { std::ostringstream s; s<<"("<<v.x<<", "<<v.y<<", "<<v.z<<", "<<v.w<<")"; return s.str(); }
	inline std::string str(const glm::quat &q) { std::ostringstream s; s<<"quat(w="<<q.w<<", "<<q.x<<", "<<q.y<<", "<<q.z<<")"; return s.str(); }

	inline float distance(float a, float b) { return std::fabs(a-b); }
	template<int N> float distance(const glm::vec<N,float,glm::defaultp> &a, const glm::vec<N,float,glm::defaultp> &b) { return glm::length(a-b); }
	//q and -q are the same rotation
	inline float distance(const glm::quat &a, const glm::quat &b) { return 1.0f-std::fabs(glm::dot(glm::normalize(a), glm::normalize(b))); }
	inline float distance(const glm::mat4 &a, const glm::mat4 &b)
	{
		float d=0.0f;
		for(int c=0; c<4; ++c) for(int r=0; r<4; ++r) d=std::max(d, std::fabs(a[c][r]-b[c][r]));
		return d;
	}

	//Silence the loaders' progress output (std::cout / std::cerr) in a scope
	struct Quiet
	{
		struct Null: std::streambuf { int overflow(int c) override { return c; } } null;
		std::streambuf *out, *err;
		Quiet(): out(std::cout.rdbuf(&null)), err(std::cerr.rdbuf(&null)) {}
		~Quiet() { std::cout.rdbuf(out); std::cerr.rdbuf(err); }
	};

	struct Test { const char *name; std::function<void()> run; };
	inline std::vector<Test> &tests() { static std::vector<Test> t; return t; }
	struct Register { Register(const char *n, std::function<void()> f) { tests().push_back({n, f}); } };

	inline int runAll(int argc, char **argv)
	{
		if(argc<2)
		{
			std::fprintf(stderr, "usage: %s TESTDATA_DIR [TEST...]\n", argv[0]);
			return 2;
		}
		dataDir()=argv[1];
		for(const Test &t : tests())
		{
			if(argc>2)
			{
				bool wanted=false;
				for(int i=2; i<argc; ++i) wanted = wanted || std::string(argv[i])==t.name;
				if(!wanted) continue;
			}
			int before=failures();
			try
			{
				t.run();
			}
			catch(const std::exception &e)
			{
				fail(__FILE__, __LINE__, std::string("unexpected exception: ")+e.what());
			}
			std::printf("%-40s %s\n", t.name, failures()==before ? "ok" : "FAILED");
		}
		std::printf("%d checks, %d failed\n", checks(), failures());
		return failures() ? 1 : 0;
	}
}

#define MMP_CAT2(a,b) a##b
#define MMP_CAT(a,b) MMP_CAT2(a,b)
//TEST(name) { ... }
#define TEST(name) static void name(); static mmptest::Register MMP_CAT(reg_,name)(#name, name); static void name()

#define CHECK(cond) do { ++mmptest::checks(); if(!(cond)) mmptest::fail(__FILE__, __LINE__, #cond); } while(0)
#define CHECK_EQ(a, b) do { ++mmptest::checks(); auto va_=(a); auto vb_=(b); if(!(va_==vb_)) \
	mmptest::fail(__FILE__, __LINE__, std::string(#a " == " #b ": ")+mmptest::str(va_)+" != "+mmptest::str(vb_)); } while(0)
#define CHECK_NEAR(a, b, tol) do { ++mmptest::checks(); auto va_=(a); auto vb_=(b); float d_=mmptest::distance(va_, vb_); \
	if(!(d_<=(tol))) mmptest::fail(__FILE__, __LINE__, std::string(#a " ~ " #b ": ")+mmptest::str(va_)+" vs "+mmptest::str(vb_)+" (difference "+mmptest::str(d_)+")"); } while(0)
//the statement must throw an exception of type E
#define CHECK_THROWS(E, stmt) do { ++mmptest::checks(); bool thrown_=false; \
	try { stmt; } catch(const E &) { thrown_=true; } catch(...) {} \
	if(!thrown_) mmptest::fail(__FILE__, __LINE__, "expected " #E " from: " #stmt); } while(0)

#define TEST_MAIN() int main(int argc, char **argv) { return mmptest::runAll(argc, argv); }

#endif

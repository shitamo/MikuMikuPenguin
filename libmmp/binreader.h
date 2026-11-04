//Bounds-checked little-endian reader over a file loaded into memory, shared
//by the PMX, PMD and VMD loaders (internal to libmmp).
//
//Reading past the end either throws std::runtime_error (Mode::Throw) or
//sets a flag and returns zeros (Mode::Flag, for formats with optional
//trailing sections).  Counts read from a file must be checked with
//checkCount() before anything is allocated for them, so that a small
//hostile file cannot make the loader allocate gigabytes.
#ifndef MMP_BINREADER_H
#define MMP_BINREADER_H

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ClosedMMDFormat
{
	//Throw a load error (also printed, like the other libmmp errors)
	[[noreturn]] inline void throwLoadError(const std::string &message)
	{
		std::cerr<<message<<std::endl;
		throw std::runtime_error(message);
	}

	//Whole file contents; throws if it cannot be opened
	inline std::vector<char> readWholeFile(const std::string &fname, const char *kind)
	{
		std::ifstream file(fname.c_str(), std::ios::in | std::ios::binary);
		if(!file) throwLoadError(std::string("ERROR: ")+kind+" file could not be found: "+fname);
		return std::vector<char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	}

	class BinaryReader
	{
		public:
		enum class Mode { Throw, Flag };

		//what: e.g. "PMX file", used in error messages together with fname
		BinaryReader(const std::vector<char> &data, Mode mode, const char *what, const std::string &fname)
			:d(data), pos(0), bad(false), mode(mode), what(what), fname(fname) {}

		bool ok() const { return !bad; }
		bool atEnd() const { return pos>=d.size(); }
		size_t remaining() const { return pos<d.size() ? d.size()-pos : 0; }
		size_t position() const { return pos; }

		[[noreturn]] void fail(const std::string &reason) const
		{
			std::ostringstream es;
			es<<"ERROR: corrupt or truncated "<<what<<" ("<<reason<<", at byte "<<pos<<"): "<<fname;
			throwLoadError(es.str());
		}

		void raw(void *out, size_t n)
		{
			if(bad || n>remaining())
			{
				if(mode==Mode::Throw) fail("unexpected end of file");
				bad=true;
				pos=d.size();
				memset(out, 0, n);
				return;
			}
			if(n) memcpy(out, &d[pos], n);
			pos+=n;
		}
		void skip(size_t n)
		{
			if(bad || n>remaining())
			{
				if(mode==Mode::Throw) fail("unexpected end of file");
				bad=true;
				pos=d.size();
				return;
			}
			pos+=n;
		}

		uint8_t  u8()  { uint8_t v;  raw(&v,1); return v; }
		uint16_t u16() { uint16_t v; raw(&v,2); return v; }
		uint32_t u32() { uint32_t v; raw(&v,4); return v; }
		int32_t  i32() { int32_t v;  raw(&v,4); return v; }
		float    f32() { float v;    raw(&v,4); return v; }
		bool     flag() { return u8()!=0; } //a byte stored in a bool (any non-zero value is true)

		//Signed index of 1, 2 or 4 bytes (-1 = none)
		int index(int size)
		{
			if(size==1) return (int8_t)u8();
			if(size==2) return (int16_t)u16();
			return i32();
		}
		//Vertex index: unsigned for 1 and 2 bytes (PMX 2.0)
		int vertexIndex(int size)
		{
			if(size==1) return u8();
			if(size==2) return u16();
			return i32();
		}

		//A count of records of at least minRecordSize bytes each: fails
		//(Throw) or returns 0 and sets the error flag (Flag) when the rest
		//of the file cannot hold that many.
		uint32_t checkCount(uint64_t count, size_t minRecordSize, const char *name)
		{
			if(bad) return 0;
			if(minRecordSize==0) minRecordSize=1;
			if(count>remaining()/minRecordSize)
			{
				if(mode==Mode::Throw) fail(std::string("invalid ")+name+" count "+std::to_string(count));
				bad=true;
				return 0;
			}
			return (uint32_t)count;
		}

		private:
		const std::vector<char> &d;
		size_t pos;
		bool bad;
		Mode mode;
		const char *what;
		std::string fname;
	};
}

#endif

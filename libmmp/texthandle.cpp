#include "texthandle.h"

#include <iconv.h>
#include <string.h>
#include <vector>

std::string sjisToUTF8(const char *sjis, std::size_t maxLen)
{
	if(!sjis) return std::string();
	
	std::size_t len=strnlen(sjis, maxLen);
	if(len==0) return std::string();
	
	//CP932 is the Windows variant of Shift_JIS that MMD actually uses;
	//fall back to plain SJIS if the iconv implementation lacks it.
	iconv_t ic=iconv_open("UTF-8", "CP932");
	if(ic==(iconv_t)-1) ic=iconv_open("UTF-8", "SJIS");
	if(ic==(iconv_t)-1) return std::string(sjis, len);
	
	std::vector<char> inbuf(sjis, sjis+len);
	std::vector<char> outbuf(len*4+4, 0);
	
	char *in=inbuf.data();
	char *out=outbuf.data();
	size_t in_size=len;
	size_t out_size=outbuf.size()-1;
	
	while(in_size>0)
	{
		if(iconv(ic, &in, &in_size, &out, &out_size)==(size_t)-1)
		{
			//skip an invalid/incomplete byte (e.g. a name truncated mid-character)
			if(in_size==0 || out_size==0) break;
			++in; --in_size;
		}
	}
	iconv_close(ic);
	
	return std::string(outbuf.data(), out-outbuf.data());
}

std::string sjisToUTF8(const char *sjis)
{
	return sjisToUTF8(sjis, MAX_BUF);
}

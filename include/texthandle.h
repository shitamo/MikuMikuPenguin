#ifndef TEXTHANDLE_H
#define TEXTHANDLE_H

#include <string>
#include <cstddef>

#define MAX_BUF 2048

//Convert a Shift_JIS string to UTF-8.  VMD files store names in fixed-size,
//not necessarily NUL-terminated fields, so pass the field size as maxLen.
std::string sjisToUTF8(const char *sjis, std::size_t maxLen);
std::string sjisToUTF8(const char *sjis);

//Convert UTF-8 to Shift_JIS (CP932); used when writing VMD files.
std::string UTF8ToSJIS(const char *utf8);

#endif

#ifndef __UTILITY_HEADER__
#define __UTILITY_HEADER__

#include <string>
#include <stdio.h>
#include <vector>
#include <errno.h>

class StringUtil
{
  public:
	static void split(const std::string& s, const std::string& delim, std::vector< std::string >* ret);
	static std::string trimLeft(const std::string& str);
	static std::string trimRight(const std::string& str);
	static std::string trim(const std::string& str);
	char* DumpHex(void* BinBuffer,char* pDumpBuffer,unsigned int DumpBufferLen);
};

#endif


#include <string>

#include "Utility.h"

//////////////////////////////////////////////////////////////////////////
//  StringUtil
//////////////////////////////////////////////////////////////////////////
void StringUtil::split(const std::string& s, const std::string& delim,std::vector<std::string>* ret)
{
	size_t last = 0;
	size_t index=s.find_first_of(delim,last);
	while (index!=std::string::npos)
	{
		ret->push_back(s.substr(last,index-last));
		last=index+1;
		index=s.find_first_of(delim,last);
	}
	if (index-last>0)
	{
		ret->push_back(s.substr(last,index-last));
	}
}

std::string StringUtil::trimLeft(const std::string& str)
{
	std::string t = str;
	for (std::string::iterator i = t.begin(); i != t.end(); i++)
	{
		if (!isspace(*i))
		{
			t.erase(t.begin(), i);
			break;
		}
	}
	return t;
}

std::string StringUtil::trimRight(const std::string& str)
{
	if (str.begin() == str.end())
	{
		return str;
	}

	std::string t = str;
	for(std::string::iterator i = t.end() - 1;;--i)
	{
		if (!isspace(*i))
		{
			t.erase(i + 1, t.end());
			break;
		}
		if(i == t.begin())
			return "";
	}
	return t;
}

std::string StringUtil::trim(const std::string& str)
{
	return trimRight(trimLeft(str));
}

static char HexTable[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

char* StringUtil::DumpHex(void* BinBuffer,char* pDumpBuffer,unsigned int DumpBufferLen)
{
	char* pTemp = pDumpBuffer;
	char* pSrc = (char*)BinBuffer;

	for(unsigned int i = 0;i < DumpBufferLen / 2;i++)
	{
		*pTemp = HexTable[pSrc[i] >> 4 & 0x0f];
		*++pTemp = HexTable[pSrc[i] & 0x0f];
		pTemp ++;
	}
	return pDumpBuffer;
}

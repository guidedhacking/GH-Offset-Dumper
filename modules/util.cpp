#include "stdafx.h"
#include "util.h"

wchar_t* TO_WCHAR_T(char* string)
{
	size_t len = strlen(string) + 1;
	wchar_t* wc_string = new wchar_t[len];
	size_t numCharsRead;
	mbstowcs_s(&numCharsRead, wc_string, len, string, _TRUNCATE);
	return wc_string;
}

char* TO_CHAR(wchar_t* string)
{
	size_t len = wcslen(string) + 1;
	char* c_string = new char[len];
	size_t numCharsRead;
	wcstombs_s(&numCharsRead, c_string, len, string, _TRUNCATE);
	return c_string;
}

std::string AddrToHexString(intptr_t addr)
{
	std::stringstream ss;
	ss << "0x" << std::uppercase << std::hex << addr;
	return ss.str();
}

size_t strfindi(std::string buffer, std::string find)
{
	std::transform(buffer.begin(), buffer.end(), buffer.begin(), std::tolower);
	std::transform(find.begin(), find.end(), find.begin(), std::tolower);
	return buffer.find(find);
}
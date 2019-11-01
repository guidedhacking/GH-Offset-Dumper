#pragma once
#include <windows.h>
#include <string>
#include <sstream>

wchar_t* TO_WCHAR_T(char* string);

char* TO_CHAR(wchar_t* string);

std::string AddrToHexString(intptr_t addr);

size_t strfindi(std::string buffer, std::string find);
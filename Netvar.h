#pragma once
#include <Windows.h>
#include "modules\ProcEx.h"
#include "modules\Mod.h"
#include "SigData.h"

const std::string LocalEntityOffsetNames[] =
{
	"m_iHealth"
};

struct NetvarData
{
	intptr_t result;
	std::string name;
	std::string prop;
	std::string table;
	int offset{ 0 };

	void Get(ProcEx proc, SigData dwGetAllClasses);

	DataType GetDataType();
	std::string GetCEVariableTypeString();
};
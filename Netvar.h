#pragma once
#include <Windows.h>
#include "modules\ProcEx.h"
#include "modules\Mod.h"
#include "SigData.h"

const std::string LocalEntityOffsetNames[] =
{
	"m_iHealth"
};

enum DataType
{
	DT_BYTE, DT_FLOAT, DT_INT, DT_INT_HEX
};

struct NetvarData
{
	intptr_t result;
	std::string name;
	std::string prop;
	std::string table;
	std::vector<int> offsets;

	void Get(ProcEx proc, SigData dwGetAllClasses);

	DataType GetDataType();
	std::string GetCEVariableTypeString();
};
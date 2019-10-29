#pragma once
#include "stdafx.h"
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <Windows.h>
#include "modules\ProcEx.h"
#include "modules\Mod.h"
#include "modules\patternscan.h"
#include "jsonxx\jsonxx.h"

jsonxx::Object ParseConfig();

struct SigData
{
	std::string name;
	int extra;
	bool relative;
	std::string module;
	std::vector<int> offsets;
	std::string comboPattern;

	intptr_t result;

	//Perform the pattern scan
	void Get(ProcEx proc);
};

class Dumper
{
public:
	jsonxx::Object* jsonConfig{ nullptr };
	ProcEx proc{};
	std::vector<SigData> signatures;

	Dumper();
	Dumper(jsonxx::Object* json);

	void ProcessSignatures();

	virtual void Dump();
};
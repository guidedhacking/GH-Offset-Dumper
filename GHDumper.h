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
#include "SigData.h"

jsonxx::Object ParseConfig();

class Dumper
{
public:
	jsonxx::Object* jsonConfig{ nullptr };
	ProcEx proc{};
	std::vector<SigData> signatures;

	Dumper();
	Dumper(jsonxx::Object* json);

	void ProcessSignatures();

	void GenerateHeaderOuput();

	virtual void Dump();
};
#include "stdafx.h"
#include "GHDumper.h"
#include "SigData.h"

jsonxx::Object ParseConfig()
{
	//Open File Stream & parse JSON data into object
	std::ifstream f("config.json", std::ios::in);
	std::string jsonBuffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	jsonxx::Object o;
	o.parse(jsonBuffer);
	return o;
}

Dumper::Dumper() {}

Dumper::Dumper(jsonxx::Object* json)
{
	jsonConfig = json;

	//Get & attach to process
	//std::string procName = jsonConfig->get<std::string>("executable");
	std::string procName = "hl2.exe";

	//Find proc & open handle
	ProcEx proc((char*)procName.c_str());
}

void Dumper::ProcessSignatures()
{
	//select signature array in json
	jsonxx::Array sigs = jsonConfig->get<jsonxx::Array>("signatures");

	//Loop through json signature array
	for (size_t i = 0; i < sigs.size(); i++)
	{
		jsonxx::Object curr = sigs.get<jsonxx::Object>(i);
		SigData currData;

		currData.name = curr.get<std::string>("name");
		currData.extra = (int)curr.get<jsonxx::Number>("extra");
		currData.relative = curr.get<jsonxx::Boolean>("relative");
		currData.module = curr.get<std::string>("module");

		//dump offsets from json into vector, not all have offsets
		jsonxx::Array offsetArray;

		//only grab them if they exist
		if (curr.has<jsonxx::Array>("offsets"))
		{
			offsetArray = curr.get<jsonxx::Array>("offsets");

			//Despite most only have 1 offset, it's an arrays
			for (size_t j = 0; j < offsetArray.size(); j++)
			{
				currData.offsets.push_back((int)offsetArray.get<jsonxx::Number>(j));
			}
		}

		currData.comboPattern = curr.get<std::string>("pattern");
		signatures.push_back(currData);
	}

	for (auto& s : signatures)
	{
		//Scan for the pattern, process the relative & extra offsets
		s.Scan(proc);
	}
}

void Dumper::GenerateHeaderOuput()
{
	//TODO: convert to string output
	std::ofstream file;
	file.open(jsonConfig->get<std::string>("filename") + ".h");

	file << "#pragma once\n#include <cstdint>\n";
	//timestamp

	file << "//GuidedHacking.com r0x0rs ur b0x0rs\n";

	file << "namespace offsets\n{\n";

	for (auto s : signatures)
	{
		file << "constexpr ptrdiff_t " << s.name << " = 0x" << std::uppercase << std::hex << s.result << ";\n";
	}

	file << "\n}\n";

	file.close();
}

void Dumper::Dump()
{
	ProcessSignatures();

	//Generate header output
	GenerateHeaderOuput();

	//Generate Cheat Engine output

	//Generate ReClass.NET output

	//Write Output files
}
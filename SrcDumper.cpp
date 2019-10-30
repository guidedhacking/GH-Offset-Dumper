#include "stdafx.h"
#include "SrcDumper.h"

void Netvar::Get(ProcEx proc, SigData dwGetAllClasses)
{
	//https://github.com/ValveSoftware/source-sdk-2013/blob/0d8dceea4310fde5706b3ce1c70609d72a38efdf/mp/src/public/client_class.h
	//https://github.com/ValveSoftware/source-sdk-2013/blob/0d8dceea4310fde5706b3ce1c70609d72a38efdf/mp/src/public/client_class.cpp
}

SrcDumper::SrcDumper() {}

SrcDumper::SrcDumper(jsonxx::Object* json)
{
	jsonConfig = json;

	//Get & attach to process
	std::string procName = jsonConfig->get<std::string>("executable");

	//Find proc & open handle
	proc = ProcEx((char*)procName.c_str());
}

intptr_t SrcDumper::GetdwGetAllClassesAddr()
{
	for (auto s : signatures)
	{
		if (s.name == "dwGetAllClasses")
		{
			return s.result;
		}
	}
	return 0;
}

HMODULE SrcDumper::LoadClientDLL(ProcEx proc)
{
	ModEx mod(_T("client_panorama.dll"), proc);
	std::filesystem::path p(mod.modEntry.szExePath);

	p = p.parent_path().parent_path().parent_path() / "bin";
	AddDllDirectory(p.wstring().c_str());

	HMODULE hMod = LoadLibraryEx(mod.modEntry.szExePath, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	return hMod;
}

intptr_t GetOffset(RecvTable* data_table, const char* netvarName)
{
	for (int i = 0; i < data_table->m_nProps; i++)
	{
		RecvProp prop = data_table->m_pProps[i];
		if (!_stricmp(prop.m_pVarName, netvarName))
		{
			return prop.m_Offset;
		}
		else if (prop.m_pDataTable)
		{
			return GetOffset(prop.m_pDataTable, netvarName);
		}
	}
	return 0;
}

intptr_t SrcDumper::GetNetVarOffset(const char* tableName, const char* netvarName, ClientClass* clientClass)
{
	ClientClass* currNode = clientClass;

	while (true)
	{
		for (int i = 0; i < currNode->m_pRecvTable->m_nProps; i++)
		{
			if (!_stricmp(currNode->m_pRecvTable->m_pProps[i].m_pVarName, netvarName))
			{
				return currNode->m_pRecvTable->m_pProps[i].m_Offset;
			}

			if (currNode->m_pRecvTable->m_pProps[i].m_pDataTable)
			{

				for (int j = 0; j < currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_nProps; j++)
				{
					if (!_stricmp(currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_pProps[j].m_pVarName, netvarName))
					{
						return currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_pProps[j].m_Offset;
					}

					if (currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_pProps[j].m_pDataTable)
					{
						for (int k = 0; k < currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_pProps[j].m_pDataTable->m_nProps; k++)
						{
							if (!_stricmp(currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_pProps[j].m_pDataTable->m_pProps[k].m_pVarName, netvarName))
							{
								return currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_pProps[j].m_pDataTable->m_pProps[k].m_Offset;
							}
						}
					}
				}
			}
		}

		if (!currNode->m_pNext) break;
		currNode = currNode->m_pNext;
	}
	return 0;
}

void SrcDumper::ProcessNetvars()
{
	jsonxx::Array netvars = jsonConfig->get<jsonxx::Array>("netvars");

	//Loop through JSON netvar array
	for (size_t i = 0; i < netvars.size(); i++)
	{
		jsonxx::Object curr = netvars.get<jsonxx::Object>(i);
		Netvar currData;

		//easy
		currData.name = curr.get<jsonxx::String>("name");//
		currData.prop = curr.get<jsonxx::String>("prop");
		currData.table = curr.get<jsonxx::String>("table");

		//dump offsets from json into vector
		if (curr.has<jsonxx::Array>("offsets"))
		{
			jsonxx::Array offsetArray = curr.get<jsonxx::Array>("offsets");

			for (size_t i = 0; i < offsetArray.size(); i++)
			{
				currData.offsets.push_back((int)offsetArray.get<jsonxx::Number>(i));
			}
		}

		Netvars.push_back(currData);
	}

	//^ we parsed the json into Netvars

	//now we can locally load client_panorama.dll and get the netvar offsets internally
	HMODULE hMod = LoadClientDLL(proc);

	//Get First ClientClass in the linked list
	ClientClass* dwGetallClassesAddr = (ClientClass*)((intptr_t)hMod + GetdwGetAllClassesAddr());

	//for each netvar in netvars, get the offset
	for (Netvar& n : Netvars)
	{
		n.addr = GetNetVarOffset(n.table.c_str(), n.prop.c_str(), dwGetallClassesAddr);
	}
}

void SrcDumper::GenerateHeaderOuput()
{
	std::ofstream file;
	file.open(jsonConfig->get<std::string>("filename") + ".h");

	file << "#pragma once\n#include <cstdint>\n";
	//timestamp

	file << "//GuidedHacking.com r0x0rs ur b0x0rs\n";

	file << "namespace offsets\n{\n";

	for (auto n : Netvars)
	{
		file << "constexpr ptrdiff_t " << n.name << " = 0x" << std::hex << n.addr << ";\n";
	}

	file << "\n//netvars\n\n";

	for (auto s : signatures)
	{
		file << "constexpr ptrdiff_t " << s.name << " = 0x" << std::hex << s.result << ";\n";
	}

	file << "\n}\n";

	file.close();
}

void SrcDumper::Dump()
{
	ProcessSignatures();

	ProcessNetvars();

	//Generate header output
	GenerateHeaderOuput();

	//Generate Cheat Engine output

	//Generate ReClass.NET output

	//Write Output files
}
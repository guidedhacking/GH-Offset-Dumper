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

/*
fails to find internally and externally:
DT_BasePlayer - m_aimPunchAngle
DT_BasePlayer - m_aimPunchAngleVel
DT_BaseAttributableItem - m_iAccountID
DT_BaseAttributableItem - m_iEntityQuality
DT_BaseCombatWeapon - m_iItemDefinitionIndex
DT_BaseAttributableItem - m_iItemIDHigh
DT_BaseAttributableItem - m_szCustomName
DT_BasePlayer - m_viewPunchAngle
DT_CSGameRulesProxy - cs_gamerules_data
DT_CSGameRulesProxy - m_SurvivalGameRuleDecisionTypes
*/

intptr_t SrcDumper::GetNetVarOffset(const char* tableName, const char* netvarName, ClientClass* clientClass)
{
	ClientClass* currNode = clientClass;
	while (true)
	{
		if (!std::strstr(currNode->m_pRecvTable->m_pNetTableName, "DT_"))
		{
			//continue;
		}

		for (int i = 0; i < currNode->m_pRecvTable->m_nProps; i++)
		{
			if (!_stricmp(currNode->m_pRecvTable->m_pProps[i].m_pVarName, netvarName))
			{
				if (currNode->m_pRecvTable->m_pProps[i].m_Offset != 0)
					return currNode->m_pRecvTable->m_pProps[i].m_Offset;
			}

			if (currNode->m_pRecvTable->m_pProps[i].m_pDataTable)
			{
				if (!strstr(currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_pNetTableName, "DT_"))
				{
					//continue;
				}

				for (int j = 0; j < currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_nProps; j++)
				{
					if (!_stricmp(currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_pProps[j].m_pVarName, netvarName))
					{
						if (currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_pProps[j].m_Offset != 0)
							return currNode->m_pRecvTable->m_pProps[i].m_pDataTable->m_pProps[j].m_Offset;
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

	int count = 0;
	//for each netvar in netvars, get the offset
	for (Netvar& n : Netvars)
	{
		n.addr = GetNetVarOffset(n.table.c_str(), n.prop.c_str(), dwGetallClassesAddr);
	}

	//debug output
	std::cout << "didn't find: " << "\n\n";

	for (Netvar& n : Netvars)
	{
		if (n.addr == 0)
		{

			std::cout << n.table << " - " << n.prop << "\n";
			count++;
		}
	}

	std::cout << "\ndid find: " << "\n\n";
	for (Netvar& n : Netvars)
	{
		std::cout << n.table << " - " << n.prop << " - 0x" << std::hex << n.addr << "\n";
	}
	//end debug output

	return;
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
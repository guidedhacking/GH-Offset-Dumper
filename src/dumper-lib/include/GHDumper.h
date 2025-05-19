#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <sstream>

#include <unordered_map>
#include <random>
#include <vector>
#include <algorithm>
#include <set>
#include <time.h>
#include <streambuf>
#include <iostream>
#include <fstream>
#include "../json.hpp"
#include "../FileScanner.h"

/* Notes
- gh::DumpNetvars() crashes if the dumper's architecture (32-bit or 64-bit) is different than the target process's architecture.
- gh::DumpNetvars() only works for games using the source engine.
- gh::DumpSignatures() and gh::DumpNetvars() return a Dump which is a std::unordered_map<std::string, ptrdiff_t>. If an error occurs, an empty map is returned.
- gh::FormatReclass() returns the Data.xml text contents. A reclass file is a <name>.rcnet zip containing Data.xml.
*/

/* Example
#include <fstream>
#include "json.hpp"
#include "GHDumper.h"

int main()
{
	// load json
	std::ifstream file("config.json"); 
	auto config = nlohmann::json::parse(file);

	// dump as std::unordered_map<std::string, ptrdiff_t>
	auto signatures = gh::DumpSignatures(config);
	auto netvars = gh::DumpNetvars(config, signatures);
	
	// format files as std::string
	auto hpp = gh::FormatHeader(config, signatures, netvars);
	auto ct = gh::FormatCheatEngine(config, signatures, netvars);
	auto xml = gh::FormatReclass(config, netvars);
	
	// save files or do whatever
	// ...
}

*/
class FileScanner;
struct DynamicModule;
struct RecvProp;
struct RecvTable
{
	RecvProp* m_pProps;
	int			m_nProps;
	void* m_pDecoder;
	char* m_pNetTableName;
	bool		m_bInitialized;
	bool		m_bInMainList;
};

struct RecvProp
{
	char* m_pVarName;
	void* m_RecvType;
	int         	m_Flags;
	int         	m_StringBufferSize;
	int         	m_bInsideArray;
	const void* m_pExtraData;
	RecvProp* m_pArrayProp;
	void* m_ArrayLengthProxy;
	void* m_ProxyFn;
	void* m_DataTableProxyFn;
	RecvTable* m_pDataTable;
	int         	m_Offset;
	int         	m_ElementStride;
	int         	m_nElements;
	const char* m_pParentArrayPropName;
};

struct ClientClass
{
	void* m_pCreateFn;
	void* m_pCreateEventFn;
	char* m_pNetworkName;
	RecvTable* m_pRecvTable;
	ClientClass* m_pNext;
	int				m_ClassID;
};
typedef std::unordered_map<std::string, ptrdiff_t> Dump;

namespace gh
{
	namespace internal
	{
		BOOL GetProcessByName(const wchar_t* processName, PROCESSENTRY32W* out);
		BOOL GetModuleByName(DWORD pid, const wchar_t* moduleName, MODULEENTRY32W* out);

		void SplitComboPattern(const char* combo, std::string& pattern, std::string& mask);

		// https://guidedhacking.com/threads/external-internal-pattern-scanning-guide.14112/
		char* ScanBasic(const char* pattern, const char* mask, char* begin, intptr_t size);
		

		// https://guidedhacking.com/threads/external-internal-pattern-scanning-guide.14112/
		char* ScanEx(const char* pattern, const char* mask, char* begin, intptr_t size, HANDLE hProc);
		
		intptr_t FindNetvarInRecvTable(RecvTable* table, const char* netvarName);
		
		intptr_t FindNetvar(const char* tableName, const char* netvarName, ClientClass* firstClientClass);
		
		std::string FormatNowUTC();
	
		nlohmann::json FindSignatureJSON(nlohmann::json& config, std::string signatureName);
		nlohmann::json FindNetvarJSON(nlohmann::json& config, std::string netvarName);
	
	} // namespace internal
		

	Dump DumpSignatures(nlohmann::json& config, FileScanner& scanner);

	Dump DumpNetvars(nlohmann::json& config, const Dump& signatures);
	
	std::string FormatHeader(nlohmann::json& config, const Dump& signatures, FileScanner& scanner, const Dump& netvars = {});
	std::string FormatReclass(nlohmann::json& config, FileScanner& scanner, const Dump& netvars = {});
	std::string FormatCheatEngine(nlohmann::json& config, const Dump& signatures, FileScanner& scanner, const Dump& netvars = {});

	FileScanner InitScanner(nlohmann::json& config, bool* runtime);
	bool ParseCommandLine(int argc, const char** argv);
} // namespace ghdumper

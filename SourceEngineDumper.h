#pragma once
#include <Windows.h>
#include <filesystem>
#include "GHDumper.h"
#include "Netvar.h"
#include "SigData.h"

// unnecessary class pointers have been converted to void* for simplicity

struct RecvProp;

struct RecvTable
{
	RecvProp*	m_pProps;
	int			m_nProps;
	void*		m_pDecoder;
	char*		m_pNetTableName;
	bool		m_bInitialized;
	bool		m_bInMainList;
};

struct RecvProp
{
	char*			m_pVarName;
	void*			m_RecvType;
	int         	m_Flags;
	int         	m_StringBufferSize;
	int         	m_bInsideArray;
	const void*		m_pExtraData;
	RecvProp*		m_pArrayProp;
	void*			m_ArrayLengthProxy;
	void*			m_ProxyFn;
	void*			m_DataTableProxyFn;
	RecvTable*  	m_pDataTable;
	int         	m_Offset;
	int         	m_ElementStride;
	int         	m_nElements;
	const char*		m_pParentArrayPropName;
};

struct ClientClass
{
	void*			m_pCreateFn;
	void*			m_pCreateEventFn;
	char*			m_pNetworkName;
	RecvTable*		m_pRecvTable;
	ClientClass*	m_pNext;
	int				m_ClassID;
};

class SourceEngineDumper : public Dumper
{
public:
	std::vector<NetvarData> Netvars;

	SourceEngineDumper();
	SourceEngineDumper(jsonxx::Object* json);

	intptr_t GetdwGetAllClassesAddr();

	HMODULE LoadClientDLL(ProcEx proc);

	intptr_t GetNetVarOffset(const char* tableName, const char* netvarName, ClientClass* clientClass);

	void ProcessNetvars();

	virtual void Dump();

	void GenerateHeaderOuput();

	void GenerateCheatEngineOutput();

	std::string GetSigBase(SigData sigdata);
};
#pragma once
#include <Windows.h>

class RecvProp;

class RecvTable
{
public:

	RecvProp*	m_pProps;
	int			m_nProps;
	void*		m_pDecoder;
	char*		m_pNetTableName;
	bool		m_bInitialized;
	bool		m_bInMainList;
};

//unnecessary class pointers have been converted to void* for simplicity

class RecvProp
{
public:
	char*					m_pVarName;
	void*					m_RecvType;
	int                     m_Flags;
	int                     m_StringBufferSize;
	int                     m_bInsideArray;
	const void*				m_pExtraData;
	RecvProp*				m_pArrayProp;
	void*					m_ArrayLengthProxy;
	void*					m_ProxyFn;
	void*					m_DataTableProxyFn;
	RecvTable* m_pDataTable;
	int                     m_Offset;
	int                     m_ElementStride;
	int                     m_nElements;
	const char*				m_pParentArrayPropName;
};

class ClientClass
{
public:
	void*			m_pCreateFn;
	void*			m_pCreateEventFn;
	char*			m_pNetworkName;
	RecvTable*		m_pRecvTable;
	ClientClass*	m_pNext;
	int				m_ClassID;
};
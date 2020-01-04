#include "stdafx.h"
#include "Netvar.h"

void NetvarData::Get(ProcEx proc, SigData dwGetAllClasses)
{
	//https://github.com/ValveSoftware/source-sdk-2013/blob/0d8dceea4310fde5706b3ce1c70609d72a38efdf/mp/src/public/client_class.h
	//https://github.com/ValveSoftware/source-sdk-2013/blob/0d8dceea4310fde5706b3ce1c70609d72a38efdf/mp/src/public/client_class.cpp
}

DataType NetvarData::GetDataType()
{
	if (name.find("m_b") != std::string::npos)
		return DataType::DT_BYTE; //boolean

	else if (name.find("m_fl") != std::string::npos)
		return DataType::DT_FLOAT;

	else if (name.find("m_vec") != std::string::npos)
		return DataType::DT_FLOAT;

	else if (strfindi(name ,"angle") != std::string::npos)
		return DataType::DT_FLOAT;

	else if (name.find("m_i") != std::string::npos)
		return DataType::DT_INT;

	else return DataType::DT_INT_HEX;
}

std::string NetvarData::GetCEVariableTypeString()
{
	std::string result;

	DataType dt = GetDataType();

	switch (dt)
	{
	//Just making a basic CE table for now, finish later
	/*
	case DataType::DT_BYTE:
		result = "<VariableType>Byte</VariableType>\n"; break;
		
	case DataType::DT_FLOAT:
		result = "<VariableType>Float</VariableType>\n"; break;

	case DataType::DT_INT_HEX:
		result = "<ShowAsHex>1</ShowAsHex>\n<VariableType>4 Bytes</VariableType>\n"; break;
	*/
	default:
		result = result + "<ShowAsHex>1</ShowAsHex>\n" + "<VariableType>4 Bytes</VariableType>\n"; break;
	}
	return result;
}
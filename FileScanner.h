#pragma once
#include "GHDumper.h"

// helper class to manage dynamicly loaded modules easier
using MappedType = std::byte;
using MappedSize = std::size_t;
class MappedFile
{
public:
	MappedType* getBytes();
	MappedSize getSize();

	explicit MappedFile() = default;
	explicit MappedFile(const std::string& diskPath);
	
	std::string getExt();
	void Release();

private:
	MappedType* mappedBytes {};
	MappedSize mappedSize {};
	std::string ext {};
};

// dynamic module structure
struct DynamicModule
{
	// file path
	std::string filePath {};
	// name used when accessing the module
	std::string compName {};
};

using DynamicMoudleArray = std::vector <DynamicModule>;

// load file from disk and dump signatures from it
class FileScanner
{
public:
	// main process will be passed as the file path
	// additional modules can also be loaded and accessed here (ex: client.dll)
	FileScanner() = default;
	FileScanner(const std::string& filePath, const DynamicMoudleArray& dynamicModules = {});
	void decon();

	MappedFile getMainFile();
	MappedFile getFileByName(const std::string& name);
	std::string getMainFileName();

	bool Valid();
private:
	// each file is mapped into our process by name
	std::unordered_map<std::string, MappedFile> mappedFiles {};
	std::string mainFileName {};
	bool valid {};

};


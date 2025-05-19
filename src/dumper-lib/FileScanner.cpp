#include "FileScanner.h"


std::vector<std::byte> FileScanner::ReadBytes(const std::string& fp)
{
    std::ifstream file_stream(fp, std::ios_base::binary);

    if (!file_stream.is_open() || !file_stream.good())
    {
        return std::vector<std::byte>();
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(file_stream)),
        std::istreambuf_iterator<char>());

    std::vector<std::byte> result(buffer.size());
    std::transform(buffer.begin(), buffer.end(), result.begin(),
        [] (char c) { return static_cast<std::byte>(c); });
    return result;
}

FileScanner::FileScanner(const std::string& filePath, const DynamicMoudleArray& dynamicModules)
{
    this->mainFileName = std::filesystem::path(filePath).filename().string();
    this->mappedFiles[mainFileName] = MappedFile(filePath);
    for (auto& module : dynamicModules)
    {
        this->mappedFiles[module.compName] = MappedFile(module.filePath);
    }
    if (this->mappedFiles[mainFileName].getBytes())
    {
        this->valid = true;
    }
}

void FileScanner::decon()
{
    for (auto& it = this->mappedFiles.begin(); it != this->mappedFiles.end(); it++)
    {
        it->second.Release();
    }
}

MappedFile FileScanner::getMainFile()
{
    return this->mappedFiles[mainFileName];
}

MappedFile FileScanner::getFileByName(const std::string& name)
{
    return this->mappedFiles[name];
}

std::string FileScanner::getMainFileName()
{
    return this->mainFileName;
}

bool FileScanner::Valid()
{
    return this->valid;
}

MappedType* MappedFile::getBytes()
{
    return this->mappedBytes;
}

MappedSize MappedFile::getSize()
{
    return this->mappedSize;
}

MappedFile::MappedFile(const std::string& diskPath)
{   
    this->ext = std::filesystem::path(diskPath).extension().string();

    const auto map = FileScanner::ReadBytes(diskPath);
    if (map.size() == 0)
    {
        return;
    }

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)map.data();
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((char*)dos + dos->e_lfanew);

    SIZE_T sizeOfImage = nt->OptionalHeader.SizeOfImage;
    this->mappedBytes = new MappedType[sizeOfImage];
    memset(this->mappedBytes, 0, sizeOfImage); 

    SIZE_T sizeOfHeaders = nt->OptionalHeader.SizeOfHeaders;
    memcpy(this->mappedBytes, map.data(), sizeOfHeaders);

    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, section++)
    {
        memcpy(this->mappedBytes + section->VirtualAddress, map.data() + section->PointerToRawData, section->SizeOfRawData);
    }

    this->mappedSize = sizeOfImage;

    printf("[+] Loaded %s at 0x%p (0x%x)\n", std::filesystem::path(diskPath).filename().string().c_str(), this->mappedBytes, this->mappedSize);
}


std::string MappedFile::getExt()
{
    return this->ext;
}

void MappedFile::Release()
{
    if (this->mappedBytes)
    {
        delete[] this->mappedBytes;
    }
}



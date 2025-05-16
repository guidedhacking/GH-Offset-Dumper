
// This project serves as an exampe to use the offset dumping library
#include <iostream>
#include <GHDumper.h>
#include <GHFileHelp.h>

int main(int argc, const char** argv)
{
	if (!gh::ParseCommandLine(argc, argv))
	{
		printf("[-] Failed to dump offsets.\n");
		Sleep(3000);
		return 1;
	}
	
	printf("[+] Successfully dumped offsets!\n");
	printf("[!] Exiting...\n");
	Sleep(3000);
	return 0;
}

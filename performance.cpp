#include "windows.h"
#include "psapi.h"

#include <iostream>

/*
// total virtual memory
MEMORYSTATUSEX memInfo;
memInfo.dwLength = sizeof(MEMORYSTATUSEX);
GlobalMemoryStatusEx(&memInfo);
DWORDLONG totalVirtualMem = memInfo.ullTotalPageFile;

// virtual memory currently used
DWORDLONG virtualMemUsed = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;

// virtual memory currently used by process
PROCESS_MEMORY_COUNTERS_EX pmc;
GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
SIZE_T virtualMemUsedByMe = pmc.PrivateUsage;
*/
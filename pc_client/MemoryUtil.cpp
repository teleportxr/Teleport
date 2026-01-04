// (C) Copyright 2018-2021 Simul Software Ltd

#include "MemoryUtil.h"
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#else
#include <sys/sysinfo.h>
#endif

// Use to convert bytes to KB
#define DIV 1024

// Specify the width of the field in which to print the numbers.
#define WIDTH 7

long PC_MemoryUtil::getAvailableMemory() const
{
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    return static_cast<long>(statex.ullAvailPhys);
#else
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    return static_cast<long>(memInfo.freeram * memInfo.mem_unit);
#endif
}

long PC_MemoryUtil::getTotalMemory() const
{
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    return static_cast<long>(statex.ullTotalPhys);
#else
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    return static_cast<long>(memInfo.totalram * memInfo.mem_unit);
#endif
}

void PC_MemoryUtil::printMemoryStats() const
{
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);

    _tprintf(TEXT("There is  %*ld percent of memory in use.\n"),
        WIDTH, statex.dwMemoryLoad);
    _tprintf(TEXT("There are %*I64d total KB of physical memory.\n"),
        WIDTH, statex.ullTotalPhys / DIV);
    _tprintf(TEXT("There are %*I64d free  KB of physical memory.\n"),
        WIDTH, statex.ullAvailPhys / DIV);
    _tprintf(TEXT("There are %*I64d total KB of paging file.\n"),
        WIDTH, statex.ullTotalPageFile / DIV);
    _tprintf(TEXT("There are %*I64d free  KB of paging file.\n"),
        WIDTH, statex.ullAvailPageFile / DIV);
    _tprintf(TEXT("There are %*I64d total KB of virtual memory.\n"),
        WIDTH, statex.ullTotalVirtual / DIV);
    _tprintf(TEXT("There are %*I64d free  KB of virtual memory.\n"),
        WIDTH, statex.ullAvailVirtual / DIV);
#else
    struct sysinfo memInfo;
    sysinfo(&memInfo);

    unsigned long totalPhysMem = memInfo.totalram * memInfo.mem_unit;
    unsigned long freePhysMem = memInfo.freeram * memInfo.mem_unit;
    unsigned long totalSwap = memInfo.totalswap * memInfo.mem_unit;
    unsigned long freeSwap = memInfo.freeswap * memInfo.mem_unit;
    long memoryLoad = 100 - (100 * freePhysMem / totalPhysMem);

    printf("There is  %*ld percent of memory in use.\n",
        WIDTH, memoryLoad);
    printf("There are %*lu total KB of physical memory.\n",
        WIDTH, totalPhysMem / DIV);
    printf("There are %*lu free  KB of physical memory.\n",
        WIDTH, freePhysMem / DIV);
    printf("There are %*lu total KB of swap space.\n",
        WIDTH, totalSwap / DIV);
    printf("There are %*lu free  KB of swap space.\n",
        WIDTH, freeSwap / DIV);
#endif
}


// (C) Copyright 2018-2021 Simul Software Ltd

#include "MemoryUtil.h"
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
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
#elif defined(__APPLE__)
    vm_size_t page_size = 0;
    mach_port_t host = mach_host_self();
    host_page_size(host, &page_size);
    vm_statistics64_data_t vmstat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vmstat, &count) != KERN_SUCCESS)
        return 0;
    return static_cast<long>(static_cast<uint64_t>(vmstat.free_count) * page_size);
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
#elif defined(__APPLE__)
    uint64_t totalMem = 0;
    size_t size = sizeof(totalMem);
    sysctlbyname("hw.memsize", &totalMem, &size, nullptr, 0);
    return static_cast<long>(totalMem);
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
#elif defined(__APPLE__)
    uint64_t totalPhysMem = static_cast<uint64_t>(getTotalMemory());
    uint64_t freePhysMem = static_cast<uint64_t>(getAvailableMemory());
    long memoryLoad = totalPhysMem ? (100 - static_cast<long>(100 * freePhysMem / totalPhysMem)) : 0;

    printf("There is  %*ld percent of memory in use.\n",
        WIDTH, memoryLoad);
    printf("There are %*llu total KB of physical memory.\n",
        WIDTH, static_cast<unsigned long long>(totalPhysMem / DIV));
    printf("There are %*llu free  KB of physical memory.\n",
        WIDTH, static_cast<unsigned long long>(freePhysMem / DIV));
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


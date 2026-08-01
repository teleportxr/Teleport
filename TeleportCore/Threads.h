#pragma once
#ifdef _MSC_VER
typedef unsigned int THREAD_TYPE;

#include <thread>
#include <windows.h>
#include <processthreadsapi.h>
#include "ErrorHandling.h"
#include "StringFunctions.h"
#include "Profiling.h"

		THREAD_TYPE GetThreadId()
		{
			return GetCurrentThreadId();
		}
		const DWORD MS_VC_SETTHREADNAME_EXCEPTION = 0x406D1388;  

#pragma pack(push,8)  
struct THREADNAME_INFO  
{  
    DWORD dwType=0x1000;	// Must be 0x1000.  
    LPCSTR szName=0;		// Pointer to name (in user addr space).  
    DWORD dwThreadID=0;		// Thread ID (-1=caller thread).  
    DWORD dwFlags=0;		// Reserved for future use, must be zero.  
 } ;  
#pragma pack(pop)

inline void SetThreadName(std::thread &thread,const char *name)
{  
	DWORD ThreadId = ::GetThreadId( static_cast<HANDLE>( thread.native_handle() ) );
    THREADNAME_INFO info;
    info.szName = name;  
    info.dwThreadID = ThreadId;
#pragma warning(push)  
#pragma warning(disable: 6320 6322)  
    __try
	{  
        RaiseException(MS_VC_SETTHREADNAME_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);  
    }  
    __except (EXCEPTION_EXECUTE_HANDLER)
	{  
    }  
#pragma warning(pop)  
}
inline void SetThisThreadName(const char *name)
{
#if 0
 SetThreadDescription(
        GetCurrentThread(),
       StringToWString(name).c_str());
    );
#else
	DWORD ThreadId = ::GetCurrentThreadId();
    THREADNAME_INFO info;
    info.szName = name;  
    info.dwThreadID = ThreadId;
#if TRACY_ENABLE
	tracy::SetThreadName(name);
#endif
#pragma warning(push)  
#pragma warning(disable: 6320 6322)  
    __try
	{  
        RaiseException(MS_VC_SETTHREADNAME_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);  
    }  
    __except (EXCEPTION_EXECUTE_HANDLER)
	{  
    }  
#pragma warning(pop)  
#endif
}


inline void SetThreadPriority(std::thread &thread,int p)
{
	DWORD nPriority=0;
	// Roderick: Upping the priority to 99 really seems to help avoid dropped packets.
	switch(p)
	{
		case -2:
		nPriority = THREAD_PRIORITY_LOWEST;
		break;
		case -1:
		nPriority = THREAD_PRIORITY_BELOW_NORMAL;
		break;
		case 0:
		nPriority = THREAD_PRIORITY_NORMAL;
		break;
		case 1:
		nPriority = THREAD_PRIORITY_ABOVE_NORMAL;
		break;
		case 2:
		nPriority = THREAD_PRIORITY_HIGHEST;
		break;
		default:
		break;
	}
	HANDLE ThreadHandle = static_cast<HANDLE>( thread.native_handle());
	BOOL result=SetThreadPriority(ThreadHandle,nPriority);
	if(!result)
	{
		TELEPORT_INTERNAL_CERR("SetThreadPriority failed\n");
	}
}
#else
#pragma once
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <cerrno>
#define THREAD_TYPE pthread_t
#include <thread>
#ifndef __APPLE__
#include <sys/prctl.h>
#endif

THREAD_TYPE GetThreadId()
{
return pthread_self();
}

// Kept in step by hand with firstparty/Platform/{Linux,MacOS}/ThisPlatform/Threads.h,
// which carry the same two functions for the Platform library.
inline void SetThreadName(std::thread& thread, const char* name)
{
#ifdef __APPLE__
	// macOS has no pthread_setname_np(pthread_t, ...): a thread can only name itself,
	// so naming another thread is not expressible. Threads stay unnamed in Instruments
	// and crash reports; this affects diagnostics only.
	(void)thread;
	(void)name;
#else
	pthread_setname_np(thread.native_handle(), name);
#endif
}

inline void SetThisThreadName(const char* name)
{
#ifdef __APPLE__
	pthread_setname_np(name);
#else
	prctl(PR_SET_NAME, (long)name, 0, 0, 0);
#endif
}

inline void SetThreadPriority(std::thread& thread, int p)
{
	sched_param sch_params;
	switch (p)
	{
	case -2:
		sch_params.sched_priority = 1;
		break;
	case -1:
		sch_params.sched_priority = 25;
		break;
	case 0:
		sch_params.sched_priority = 50;
		break;
	case 1:
		sch_params.sched_priority = 75;
		break;
	case 2:
		sch_params.sched_priority = 99;
		break;
	default:
		sch_params.sched_priority = 50;
		break;
	}
	pthread_setschedparam(thread.native_handle(), SCHED_RR, &sch_params);
}

inline int fopen_s(FILE** pFile, const char* filename, const char* mode)
{
	*pFile = fopen(filename, mode);
	return errno;
}

inline int fdopen_s(FILE** pFile, int fildes, const char* mode)
{
	*pFile = fdopen(fildes, mode);
	return errno;
}
#endif
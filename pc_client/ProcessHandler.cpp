#include "ProcessHandler.h"
#ifdef _MSC_VER
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <psapi.h>
#include <fmt/core.h>
#include <iostream>
#include <vector>
#include <algorithm>

// To ensure correct resolution of symbols, add Psapi.lib to TARGETLIBS
// and compile with -DPSAPI_VERSION=1

std::string GetProcessName( DWORD processID )
{
    char szProcessName[MAX_PATH] = "<unknown>";

    // Get a handle to the process.

    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );

    // Get the process name.

    if (NULL != hProcess )
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), 
             &cbNeeded) )
        {
            GetModuleBaseNameA( hProcess, hMod, szProcessName, 
                               sizeof(szProcessName)/sizeof(TCHAR) );
        }
    }

    // Print the process name and identifier.

    std::string ret=szProcessName;

    // Release the handle to the process.

    CloseHandle( hProcess );
	return ret;
}
#define MYDISPLAY 1
typedef struct tagMYREC
{
	char s1[500];
	DWORD n;
} MYREC;
#endif

#define APPLICATION_INSTANCE_MUTEX_NAME "{FA43C45E-B29A-4359-A07C-51B65B5571AD}"
bool EnsureSingleProcess(const std::string &cmdLine)
{
#ifdef _MSC_VER
    // A Mutex to prevent multiple instances:

	// Make sure at most one instance of the tool is running
	HANDLE hMutexOneInstance(::CreateMutexA(NULL, TRUE, APPLICATION_INSTANCE_MUTEX_NAME));
	bool bAlreadyRunning((::GetLastError() == ERROR_ALREADY_EXISTS));
	if (hMutexOneInstance != nullptr && !bAlreadyRunning)
	{
		return false;
	}
	// Can't lock the mutex so the app is running already.
	if (hMutexOneInstance)
	{
		::ReleaseMutex(hMutexOneInstance);
		::CloseHandle(hMutexOneInstance);
	}
	// Get the list of process identifiers.

	std::vector<DWORD> aProcesses(1024);
	DWORD cbNeeded=0, cProcesses=0;
	unsigned int i;
	if (!EnumProcesses(aProcesses.data(), (DWORD)aProcesses.size() * sizeof(DWORD), &cbNeeded))
	{
		return false;
	}
	// Calculate how many process identifiers were returned.
	cProcesses = cbNeeded / sizeof(DWORD);
	if (cProcesses > aProcesses.size())
	{
		aProcesses.resize(cProcesses);
		if (!EnumProcesses(aProcesses.data(), (DWORD)aProcesses.size() * sizeof(DWORD), &cbNeeded))
		{
			return false;
		}
	}
	DWORD currentProcessId = GetCurrentProcessId();
	if(!currentProcessId)
		return false;
	std::string currentProcessName = GetProcessName(currentProcessId);
	if(currentProcessName.length()==0)
		return false;
	DWORD existingProcessId = 0;
	// Print the name and process identifier for each process.
	for (i=0;i<cProcesses;i++)
	{
		if(currentProcessId==aProcesses[i])
			continue;
		if(GetProcessName(aProcesses[i])==currentProcessName)
		{
			// This is the process we want. Send it the command line.
			existingProcessId = aProcesses[i];
			break;
		}
	}
	if(!existingProcessId)
		return false;
// send the command line to the specified process.
	MYREC MyRec;
	memcpy(MyRec.s1,cmdLine.c_str(),std::min((size_t)499,cmdLine.length()));
	MyRec.s1[499]=0;
	COPYDATASTRUCT MyCDS;
	MyCDS.dwData = TELEPORT_COMMAND_LINE;		  // function identifier
	MyCDS.cbData = sizeof(MyRec); // size of data
	MyCDS.lpData = &MyRec;		  // data structure
//
	HWND hwDispatch = FindWindowA("TeleportPCClientWindowClass", "Teleport VR Client");
	if (hwDispatch != NULL)
	{
		SendMessageA(hwDispatch,
					WM_COPYDATA,
					(WPARAM)(HWND)0,
					(LPARAM)(LPVOID)&MyCDS);
	}
	else
		return false;
	return true;
#endif
	return false;
}

std::string GetExternalCommandLine(__int64 lParam)
{
	const COPYDATASTRUCT *pMyCDS = (const COPYDATASTRUCT *)lParam;
	if(!pMyCDS)
		return "";
	if(pMyCDS->dwData!=TELEPORT_COMMAND_LINE)
		return "";
	if (pMyCDS->cbData!=sizeof(MYREC))
		return "";
	MYREC *myRec = (MYREC *)pMyCDS->lpData;
	if(!myRec)
		return "";
	myRec->s1[499]=0;
	std::string ret = myRec->s1;
	return ret;
}

#include "V8ProcessManager.h"


	bool V8ProcessManager::CreateTabProcess(uint32_t tabId, bool debugChildProcess)
	{
		SECURITY_ATTRIBUTES sa;
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = NULL;

		// Create pipe for IPC
		std::string uniquePipeName = "\\\\.\\pipe\\v8browser_" + std::to_string(tabId);
		HANDLE pipe = CreateNamedPipeA(
			uniquePipeName.c_str(),
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			1,
			PIPE_BUFFER_SIZE,
			PIPE_BUFFER_SIZE,
			0,
            &sa
        );

        if (pipe == INVALID_HANDLE_VALUE) return false;

		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		// Command line includes tab ID and pipe name
		std::string cmdLine = "JavaScriptWorker.exe " + std::to_string(tabId) + " " + uniquePipeName;

		if (debugChildProcess)
			cmdLine += " WAIT_FOR_DEBUGGER";

		BOOL isProcessInJob=false;
		bool success=IsProcessInJob( GetCurrentProcess(), NULL, &isProcessInJob );
		if(!success)
		{
	        return false;
	    }
		if (isProcessInJob)
		{
			std::cout<<"Process is already in Job\n";
		}
		DWORD dwCreationFlags = CREATE_NEW_CONSOLE|(isProcessInJob ? CREATE_BREAKAWAY_FROM_JOB : 0);
		if (!CreateProcessA(
				NULL,
				const_cast<LPSTR>(cmdLine.c_str()),
				NULL,
				NULL,
				TRUE,
				CREATE_SUSPENDED,
				NULL,
				NULL,
				&si,
				&pi))
		{
			long errorMessageID = GetLastError();
			LPSTR messageBuffer = nullptr;

			//Ask Win32 to give us the string version of that message ID.
			//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
			size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
										 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    
			//Copy the error message into a std::string.
			std::string message(messageBuffer, size);
			std::cerr<<"CreateProcessA failed with: "<<message<<"\n";
			CloseHandle(pipe);
			return false;
		}
        // Assign process to browser job object
        if (browserJobObject && !AssignProcessToJobObject(browserJobObject, pi.hProcess)) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(pipe);
            return false;
        }


        ResumeThread(pi.hThread);

		TabProcess tp = {
			pi.hProcess,
			pipe,
			pi.dwProcessId,
            true
		};

		tabProcesses[tabId] = tp;
		CloseHandle(pi.hThread);
		return true;
	}
	bool debugChildProcesses = false;

V8ProcessManager::V8ProcessManager(bool deb)
	{
		debugChildProcesses = deb;
        // Create main job object
        browserJobObject = CreateJobObjectA(NULL, NULL);
        if (browserJobObject) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(browserJobObject, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        }
	}

V8ProcessManager::~V8ProcessManager()
	{
		// Cleanup all processes
		for (auto &pair : tabProcesses)
		{
			TerminateTab(pair.first);
		}
        if (browserJobObject)
			CloseHandle(browserJobObject);
	}

	// Initialize a new tab with V8 instance
	bool V8ProcessManager::InitializeTab(uint32_t tabId)
	{
		if (tabProcesses.find(tabId) != tabProcesses.end())
		{
			return false; // Tab already exists
		}
		return CreateTabProcess(tabId, debugChildProcesses);
	}
	bool V8ProcessManager::IsReady(uint32_t tabId)
	{
		// Read result from pipe
		auto it = tabProcesses.find(tabId);
		if (it == tabProcesses.end() || !it->second.isRunning)
		{
			return false;
		}
		char buffer[PIPE_BUFFER_SIZE+1];
		DWORD bytesRead;
		if (!ReadFile(
				it->second.pipeHandle,
				buffer,
				PIPE_BUFFER_SIZE,
				&bytesRead,
				NULL))
		{
			return false;
		}
		buffer[bytesRead]=0;
		if(std::string(buffer)!="ready\n")
			return false;
		return true;
	}
	// Execute JavaScript in specific tab
	bool V8ProcessManager::ExecuteScript(uint32_t tabId, const std::string &script)
	{
		auto it = tabProcesses.find(tabId);
		if (it == tabProcesses.end() || !it->second.isRunning)
		{
			return false;
		}

		// Write script to pipe
		DWORD bytesWritten;
		if (!WriteFile(
				it->second.pipeHandle,
				script.c_str(),
				static_cast<DWORD>(script.length()),
				&bytesWritten,
				NULL))
		{
			return false;
		}

		// Read result from pipe
		char buffer[PIPE_BUFFER_SIZE+1];
		DWORD bytesRead;
		if (!ReadFile(
				it->second.pipeHandle,
				buffer,
				PIPE_BUFFER_SIZE,
				&bytesRead,
				NULL))
		{
			return false;
		}
		buffer[bytesRead]=0;
		std::cout<<buffer;
		return true;
	}

	// Terminate a tab's V8 process
	bool V8ProcessManager::TerminateTab(uint32_t tabId)
	{
		auto it = tabProcesses.find(tabId);
		if (it == tabProcesses.end())
		{
			return false;
		}

		TerminateProcess(it->second.processHandle, 0);
		CloseHandle(it->second.processHandle);
		CloseHandle(it->second.pipeHandle);
		it->second.isRunning = false;
		tabProcesses.erase(it);
		return true;
	}

	// Check if tab process is still running
	bool V8ProcessManager::IsTabRunning(uint32_t tabId)
	{
		auto it = tabProcesses.find(tabId);
		if (it == tabProcesses.end())
		{
			return false;
		}

		DWORD exitCode;
		if (!GetExitCodeProcess(it->second.processHandle, &exitCode))
		{
			return false;
		}
		return exitCode == STILL_ACTIVE;
	}

	// Get number of active tab processes
	size_t V8ProcessManager::GetActiveTabCount() const
	{
		return tabProcesses.size();
	}
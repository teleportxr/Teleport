#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <iterator>

#ifdef _WIN32
#include <windows.h>
using ProcessHandle = HANDLE;
using PipeHandle = HANDLE;
using ProcessId = DWORD;
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
using ProcessHandle = pid_t;
using PipeHandle = int;
using ProcessId = pid_t;
#endif


class V8ProcessManager
{
private:
	struct TabProcess
	{
		ProcessHandle processHandle;
		PipeHandle pipeReadHandle;
		PipeHandle pipeWriteHandle;
		ProcessId processId;
		bool isRunning;
#ifdef _WIN32
		HANDLE jobObject;
#endif
	};

	std::unordered_map<uint32_t, TabProcess> tabProcesses;
	std::string pipeName;
#ifdef _WIN32
	HANDLE browserJobObject;
#endif
	static const size_t PIPE_BUFFER_SIZE = 4096;

	// Create a new process for a tab
	bool CreateTabProcess(uint32_t tabId, bool debugChildProcess = false);
	bool debugChildProcesses = false;

public:
	V8ProcessManager(bool deb = false);

	~V8ProcessManager();

	// Initialize a new tab with V8 instance
	bool InitializeTab(uint32_t tabId);
	bool IsReady(uint32_t tabId);
	// Execute JavaScript in specific tab
	bool ExecuteScript(uint32_t tabId, const std::string &script);

	// Terminate a tab's V8 process
	bool TerminateTab(uint32_t tabId);

	// Check if tab process is still running
	bool IsTabRunning(uint32_t tabId);

	// Get number of active tab processes
	size_t GetActiveTabCount() const;
};
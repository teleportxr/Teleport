#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <fstream>
#include <iterator>

#pragma optimize("", off)
class V8ProcessManager
{
private:
	struct TabProcess
	{
		HANDLE processHandle;
		HANDLE pipeHandle;
		DWORD processId;
		bool isRunning;
        HANDLE jobObject;
	};

	std::unordered_map<uint32_t, TabProcess> tabProcesses;
	std::string pipeName;
    HANDLE browserJobObject;
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
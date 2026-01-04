#include "V8ProcessManager.h"

#ifdef _WIN32

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

	BOOL isProcessInJob = false;
	bool success = IsProcessInJob(GetCurrentProcess(), NULL, &isProcessInJob);
	if (!success)
	{
		return false;
	}
	if (isProcessInJob)
	{
		std::cout << "Process is already in Job\n";
	}
	DWORD dwCreationFlags = CREATE_NEW_CONSOLE | (isProcessInJob ? CREATE_BREAKAWAY_FROM_JOB : 0);
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

		size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
									 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

		std::string message(messageBuffer, size);
		std::cerr << "CreateProcessA failed with: " << message << "\n";
		CloseHandle(pipe);
		return false;
	}
	// Assign process to browser job object
	if (browserJobObject && !AssignProcessToJobObject(browserJobObject, pi.hProcess))
	{
		TerminateProcess(pi.hProcess, 1);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(pipe);
		return false;
	}

	ResumeThread(pi.hThread);

	TabProcess tp = {};
	tp.processHandle = pi.hProcess;
	tp.pipeReadHandle = pipe;
	tp.pipeWriteHandle = pipe;  // Same handle for Windows named pipe
	tp.processId = pi.dwProcessId;
	tp.isRunning = true;

	tabProcesses[tabId] = tp;
	CloseHandle(pi.hThread);
	return true;
}

V8ProcessManager::V8ProcessManager(bool deb)
{
	debugChildProcesses = deb;
	// Create main job object
	browserJobObject = CreateJobObjectA(NULL, NULL);
	if (browserJobObject)
	{
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
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

#else // Unix-like systems

bool V8ProcessManager::CreateTabProcess(uint32_t tabId, bool debugChildProcess)
{
	// Create pipes for bidirectional IPC
	// parentToChild: parent writes, child reads
	// childToParent: child writes, parent reads
	int parentToChild[2];
	int childToParent[2];

	if (pipe(parentToChild) == -1)
	{
		std::cerr << "Failed to create parentToChild pipe\n";
		return false;
	}
	if (pipe(childToParent) == -1)
	{
		std::cerr << "Failed to create childToParent pipe\n";
		close(parentToChild[0]);
		close(parentToChild[1]);
		return false;
	}

	pid_t pid = fork();
	if (pid == -1)
	{
		std::cerr << "Fork failed\n";
		close(parentToChild[0]);
		close(parentToChild[1]);
		close(childToParent[0]);
		close(childToParent[1]);
		return false;
	}

	if (pid == 0)
	{
		// Child process
		// Close unused ends of pipes
		close(parentToChild[1]);  // Close write end of parentToChild
		close(childToParent[0]);  // Close read end of childToParent

		// Redirect stdin to parentToChild read end
		dup2(parentToChild[0], STDIN_FILENO);
		// Redirect stdout to childToParent write end
		dup2(childToParent[1], STDOUT_FILENO);

		close(parentToChild[0]);
		close(childToParent[1]);

		// Build command line arguments
		std::string tabIdStr = std::to_string(tabId);
		std::vector<char*> args;
		args.push_back(const_cast<char*>("JavaScriptWorker"));
		args.push_back(const_cast<char*>(tabIdStr.c_str()));
		args.push_back(const_cast<char*>("STDIO"));  // Indicate we're using stdio for IPC

		if (debugChildProcess)
		{
			args.push_back(const_cast<char*>("WAIT_FOR_DEBUGGER"));
		}
		args.push_back(nullptr);

		execvp("./JavaScriptWorker", args.data());

		// If exec fails
		std::cerr << "execvp failed\n";
		_exit(1);
	}

	// Parent process
	// Close unused ends of pipes
	close(parentToChild[0]);  // Close read end of parentToChild
	close(childToParent[1]);  // Close write end of childToParent

	TabProcess tp = {};
	tp.processHandle = pid;
	tp.pipeWriteHandle = parentToChild[1];  // Parent writes to child via this
	tp.pipeReadHandle = childToParent[0];   // Parent reads from child via this
	tp.processId = pid;
	tp.isRunning = true;

	tabProcesses[tabId] = tp;
	return true;
}

V8ProcessManager::V8ProcessManager(bool deb)
{
	debugChildProcesses = deb;
	// No job object equivalent needed for Unix - child processes are managed via signals
}

V8ProcessManager::~V8ProcessManager()
{
	// Cleanup all processes
	for (auto &pair : tabProcesses)
	{
		TerminateTab(pair.first);
	}
}

#endif // _WIN32

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
	char buffer[PIPE_BUFFER_SIZE + 1];
#ifdef _WIN32
	DWORD bytesRead;
	if (!ReadFile(
			it->second.pipeReadHandle,
			buffer,
			PIPE_BUFFER_SIZE,
			&bytesRead,
			NULL))
	{
		return false;
	}
#else
	ssize_t bytesRead = read(it->second.pipeReadHandle, buffer, PIPE_BUFFER_SIZE);
	if (bytesRead <= 0)
	{
		return false;
	}
#endif
	buffer[bytesRead] = 0;
	if (std::string(buffer) != "ready\n")
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

#ifdef _WIN32
	// Write script to pipe
	DWORD bytesWritten;
	if (!WriteFile(
			it->second.pipeWriteHandle,
			script.c_str(),
			static_cast<DWORD>(script.length()),
			&bytesWritten,
			NULL))
	{
		return false;
	}

	// Read result from pipe
	char buffer[PIPE_BUFFER_SIZE + 1];
	DWORD bytesRead;
	if (!ReadFile(
			it->second.pipeReadHandle,
			buffer,
			PIPE_BUFFER_SIZE,
			&bytesRead,
			NULL))
	{
		return false;
	}
#else
	// Write script to pipe
	ssize_t bytesWritten = write(it->second.pipeWriteHandle, script.c_str(), script.length());
	if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) != script.length())
	{
		return false;
	}

	// Read result from pipe
	char buffer[PIPE_BUFFER_SIZE + 1];
	ssize_t bytesRead = read(it->second.pipeReadHandle, buffer, PIPE_BUFFER_SIZE);
	if (bytesRead <= 0)
	{
		return false;
	}
#endif
	buffer[bytesRead] = 0;
	std::cout << buffer;
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

#ifdef _WIN32
	TerminateProcess(it->second.processHandle, 0);
	CloseHandle(it->second.processHandle);
	CloseHandle(it->second.pipeReadHandle);
	// pipeWriteHandle is same as pipeReadHandle on Windows (named pipe)
#else
	kill(it->second.processHandle, SIGTERM);
	waitpid(it->second.processHandle, nullptr, 0);
	close(it->second.pipeReadHandle);
	close(it->second.pipeWriteHandle);
#endif
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

#ifdef _WIN32
	DWORD exitCode;
	if (!GetExitCodeProcess(it->second.processHandle, &exitCode))
	{
		return false;
	}
	return exitCode == STILL_ACTIVE;
#else
	int status;
	pid_t result = waitpid(it->second.processHandle, &status, WNOHANG);
	if (result == 0)
	{
		// Process is still running
		return true;
	}
	else if (result == it->second.processHandle)
	{
		// Process has exited
		it->second.isRunning = false;
		return false;
	}
	else
	{
		// Error occurred
		return false;
	}
#endif
}

// Get number of active tab processes
size_t V8ProcessManager::GetActiveTabCount() const
{
	return tabProcesses.size();
}
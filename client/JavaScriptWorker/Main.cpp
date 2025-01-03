#include "libplatform/libplatform.h"
#include "v8.h"
#include <iostream>
#include <string>
#include <windows.h>
#pragma optimize("", off)
class V8Worker
{
private:
	HANDLE pipeHandle;
	std::unique_ptr<v8::Platform> platform;
	v8::Isolate *isolate;
	v8::Global<v8::Context> context;
	static const size_t PIPE_BUFFER_SIZE = 4096;

	void InitializeV8(const std::string &exec_name)
	{
		// Initialize V8
		v8::V8::InitializeICUDefaultLocation(exec_name.c_str());
		v8::V8::InitializeExternalStartupData(exec_name.c_str());
		platform = v8::platform::NewDefaultPlatform();
		v8::V8::InitializePlatform(platform.get());
		v8::V8::Initialize();

		// Create isolate and context
		v8::Isolate::CreateParams create_params;
		create_params.array_buffer_allocator =
			v8::ArrayBuffer::Allocator::NewDefaultAllocator();

		isolate = v8::Isolate::New(create_params);

		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);

		// Create global object template
		v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

		// Add any custom functions to global object here
		// Example: global->Set(v8::String::NewFromUtf8(isolate, "log").ToLocalChecked(),
		//                     v8::FunctionTemplate::New(isolate, LogCallback));

		// Create context and persist it
		v8::Local<v8::Context> ctx = v8::Context::New(isolate, nullptr, global);
		context.Reset(isolate, ctx);
	}

	std::string ExecuteScript(const std::string &script)
	{
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);

		v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, context);
		v8::Context::Scope context_scope(ctx);

		try
		{
			// Compile script
			v8::Local<v8::String> source =
				v8::String::NewFromUtf8(isolate, script.c_str())
					.ToLocalChecked();

			v8::Local<v8::Script> compiled_script;
			v8::TryCatch try_catch(isolate);

			if (!v8::Script::Compile(ctx, source).ToLocal(&compiled_script))
			{
				v8::String::Utf8Value error(isolate, try_catch.Exception());
				return "Compilation error: " + std::string(*error);
			}

			// Execute script
			v8::Local<v8::Value> result;
			if (!compiled_script->Run(ctx).ToLocal(&result))
			{
				v8::String::Utf8Value error(isolate, try_catch.Exception());
				return "Execution error: " + std::string(*error);
			}

			// Convert result to string
			v8::String::Utf8Value utf8(isolate, result);
			return *utf8;
		}
		catch (const std::exception &e)
		{
			return "Exception: " + std::string(e.what());
		}
	}
	bool Write(const std::string &output) const
	{
		// Send result back through pipe
		DWORD bytesWritten;
		if (!WriteFile(pipeHandle, output.c_str(),
					   static_cast<DWORD>(output.length()),
					   &bytesWritten, NULL))
		{
			std::cerr << "Failed to write to pipe: " << GetLastError() << std::endl;
			return false;
		}
		return true;
	}
	void ProcessMessages()
	{
		char buffer[PIPE_BUFFER_SIZE];
		DWORD bytesRead;
		Write("ready\n");
		while (true)
		{
			// Read script from pipe
			if (!ReadFile(pipeHandle, buffer, PIPE_BUFFER_SIZE, &bytesRead, NULL))
			{
				std::cerr << "Failed to read from pipe: " << GetLastError() << std::endl;
				break;
			}

			if (bytesRead == 0)
			{
				continue;
			}

			// Execute script
			std::string script(buffer, bytesRead);
			std::string result = ExecuteScript(script);
			if(!Write(result))
				break;
		}
	}

public:
	V8Worker(const std::string &execName, const std::string &pipeName)
	{
		// Connect to named pipe
		while (true)
		{
			pipeHandle = CreateFileA(
				pipeName.c_str(),
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				OPEN_EXISTING,
				0,
				NULL);

			if (pipeHandle != INVALID_HANDLE_VALUE)
			{
				break;
			}

			if (GetLastError() != ERROR_PIPE_BUSY)
			{
				throw std::runtime_error("Could not open pipe");
			}

			// Wait for pipe to become available
			if (!WaitNamedPipeA(pipeName.c_str(), 5000))
			{
				throw std::runtime_error("Could not open pipe: timeout");
			}
		}

		// Initialize V8
		InitializeV8(execName);
	}

	~V8Worker()
	{
		// Cleanup V8
		context.Reset();
		isolate->Dispose();
		v8::V8::Dispose();
		v8::V8::ShutdownPlatform();

		// Close pipe
		if (pipeHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(pipeHandle);
		}
	}

	void Run()
	{
		ProcessMessages();
	}
};
#pragma optimize("", off)
int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		static char *default_arg[3] = {argv[0], "1", "Test"};
		argc = 3;
		argv = default_arg;
		// std::cerr << "Usage: v8worker.exe <tabId> <pipeName>" << std::endl;
		// return 1;
	}
#ifdef _MSC_VER
	if (argc >= 4)
	{
		bool pause_for_debugger = false;
		if (std::string(argv[3]) == "WAIT_FOR_DEBUGGER")
			Sleep(20000);
	}
#endif
	try
	{
		std::string execName = argv[0];
		std::string pipeName = argv[2];
		// Create and run worker
		V8Worker worker(execName, pipeName);
		worker.Run();
		return 0;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}
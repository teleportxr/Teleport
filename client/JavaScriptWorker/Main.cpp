#include "SceneObjectModel.h"
#include "libplatform/libplatform.h"
#include "v8.h"
#include <iostream>
#include <string>

#pragma optimize("", off)

class V8Worker
{
private:
	struct CallbackData 
	{
		V8Worker* worker;
		std::shared_ptr<Scene> scene;
	};
	HANDLE pipeHandle;
	std::unique_ptr<v8::Platform> platform;
	v8::Isolate *isolate = nullptr;
	v8::Global<v8::Context> context;
	std::shared_ptr<Scene> scene;
    v8::Global<v8::ObjectTemplate> nodeTemplate; 
	static const size_t PIPE_BUFFER_SIZE = 4096;
		
	static void LogCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
	{
		if (info.Length() < 1)
			return;
		v8::Isolate* isolate = info.GetIsolate();
		v8::HandleScope scope(isolate);
		v8::Local<v8::Value> arg = info[0];
		v8::String::Utf8Value value(isolate, arg);
		std::string str=value.operator*();
		std::cout<<str<<"\n";
		info.GetReturnValue().SetEmptyString();
	}
    static void GetChildrenCallback(v8::Local<v8::String> property,
                                  const v8::PropertyCallbackInfo<v8::Value>& info)
    {
        v8::Isolate* isolate = info.GetIsolate();
        v8::HandleScope handleScope(isolate);
        
        auto self = info.Holder();
        auto wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
        auto node = static_cast<SOMNode*>(wrap->Value());

        auto data = static_cast<CallbackData*>(
            v8::Local<v8::External>::Cast(info.Data())->Value()
        );

        const auto& childNodes = node->getChildNodes();
        info.GetReturnValue().Set(data->worker->WrapNodeArray(childNodes));
    }

	static std::string ToString(v8::Isolate *isolate, v8::Local<v8::Value> value)
	{
		v8::String::Utf8Value utf8(isolate, value);
		return *utf8 ? *utf8 : "";
	}
	static uint64_t ToUid(v8::Local<v8::Value> value)
	{
		if (value->IsUndefined() || value->IsNull())
		{
			return 0;
		}

		if (value->IsBigInt())
		{
			bool lossless;
			return value.As<v8::BigInt>()->Uint64Value(&lossless);
		}

		if (value->IsInt32())
		{
			bool lossless;
			return value.As<v8::BigInt>()->Uint64Value(&lossless);
		}

		return 0;
	}

	v8::Local<v8::Object> WrapPose(const Pose &pose)
	{
		v8::EscapableHandleScope handle_scope(isolate);
		v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, context);
		v8::Context::Scope context_scope(ctx);

		v8::Local<v8::Object> obj = v8::Object::New(isolate);

		obj->Set(ctx, v8::String::NewFromUtf8(isolate, "x").ToLocalChecked(),
				 v8::Number::New(isolate, pose.position.x))
			.Check();
		obj->Set(ctx, v8::String::NewFromUtf8(isolate, "y").ToLocalChecked(),
				 v8::Number::New(isolate, pose.position.y))
			.Check();
		obj->Set(ctx, v8::String::NewFromUtf8(isolate, "z").ToLocalChecked(),
				 v8::Number::New(isolate, pose.position.z))
			.Check();
		obj->Set(ctx, v8::String::NewFromUtf8(isolate, "qx").ToLocalChecked(),
				 v8::Number::New(isolate, pose.orientation.x))
			.Check();
		obj->Set(ctx, v8::String::NewFromUtf8(isolate, "qy").ToLocalChecked(),
				 v8::Number::New(isolate, pose.orientation.y))
			.Check();
		obj->Set(ctx, v8::String::NewFromUtf8(isolate, "qz").ToLocalChecked(),
				 v8::Number::New(isolate, pose.orientation.z))
			.Check();
		obj->Set(ctx, v8::String::NewFromUtf8(isolate, "qw").ToLocalChecked(),
				 v8::Number::New(isolate, pose.orientation.w))
			.Check();

		return handle_scope.Escape(obj);
	}
	//! Convert a JavaScript to a C++ Pose.
	static Pose PoseFromObject(v8::Isolate *isolate,v8::Local<v8::Object> obj)
	{
		v8::Local<v8::Context> ctx(isolate->GetCurrentContext());
		//v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, context);
		Pose pose;

		auto getNumber = [&](const char *prop) -> double
		{
			return obj->Get(ctx, v8::String::NewFromUtf8(isolate, prop).ToLocalChecked())
				.ToLocalChecked()
				->NumberValue(ctx)
				.ToChecked();
		};

		pose.position.x = getNumber("x");
		pose.position.y = getNumber("y");
		pose.position.z = getNumber("z");
		pose.orientation.x = getNumber("qx");
		pose.orientation.y = getNumber("qy");
		pose.orientation.z = getNumber("qz");
		pose.orientation.w = getNumber("qw");

		return pose;
	}

	v8::Local<v8::Object> WrapNode(std::shared_ptr<SOMNode> node)
	{
	#if 1
	
        v8::EscapableHandleScope handle_scope(isolate);
        v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, context);
        v8::Context::Scope context_scope(ctx);

        // Create new instance from template
        v8::Local<v8::ObjectTemplate> templ = v8::Local<v8::ObjectTemplate>::New(isolate, nodeTemplate);
        v8::Local<v8::Object> obj = templ->NewInstance(ctx).ToLocalChecked();
        
        // Store the C++ pointer
        obj->SetInternalField(0, v8::External::New(isolate, node.get()));

        // Set basic non-accessor properties
        obj->Set(ctx,
            v8::String::NewFromUtf8(isolate, "nodeName").ToLocalChecked(),
            v8::String::NewFromUtf8(isolate, node->getNodeName().c_str()).ToLocalChecked()
        ).Check();

        return handle_scope.Escape(obj);
	#else
		v8::EscapableHandleScope handle_scope(isolate);
		v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, context);
		v8::Context::Scope context_scope(ctx);
		
        // Get the node template
        v8::Local<v8::ObjectTemplate> templ = v8::Local<v8::ObjectTemplate>::New(isolate, nodeTemplate);
        
        // Create new instance from template
        v8::Local<v8::Object> obj = templ->NewInstance(ctx).ToLocalChecked();
		
		auto fld=v8::External::New(isolate, node.get());

		obj->SetInternalField(0,fld );
		// Add basic properties (excluding children which is handled by accessor)
        // Add UID as BigInt since it's 64-bit
        obj->Set(ctx,
            v8::String::NewFromUtf8(isolate, "uid").ToLocalChecked(),
            v8::BigInt::NewFromUnsigned(isolate, node->getUid())
        ).Check();
        obj->Set(ctx,
            v8::String::NewFromUtf8(isolate, "nodeName").ToLocalChecked(),
            v8::String::NewFromUtf8(isolate, node->getNodeName().c_str()).ToLocalChecked()
        ).Check();

        obj->Set(ctx,
            v8::String::NewFromUtf8(isolate, "pose").ToLocalChecked(),
            WrapPose(node->getLocalPose())
        ).Check();
		
		// Add methods
		auto setPoseTpl = v8::FunctionTemplate::New(isolate,
													[](const v8::FunctionCallbackInfo<v8::Value> &args)
													{
														if (args.Length() < 1)
															return;
														v8::Isolate* isol = args.GetIsolate();
														auto self = args.Holder();
														auto wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
														SOMNode *nodeptr = static_cast<SOMNode *>(wrap->Value());

														auto poseObj = args[0]->ToObject(args.GetIsolate()->GetCurrentContext()).ToLocalChecked();
														nodeptr->setPose(PoseFromObject(isol,poseObj));
													});

		obj->Set(ctx,
				 v8::String::NewFromUtf8(isolate, "setPose").ToLocalChecked(),
				 setPoseTpl->GetFunction(ctx).ToLocalChecked())
			.Check();

		// Add other methods similarly...
		return handle_scope.Escape(obj);
		#endif
	}
    v8::Local<v8::Array> WrapNodeArray(const std::vector<std::shared_ptr<SOMNode>>& nodes)
    {
        v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, context);
        v8::Local<v8::Array> array = v8::Array::New(isolate, static_cast<int>(nodes.size()));
        
        for (size_t i = 0; i < nodes.size(); ++i) 
        {
            array->Set(ctx, static_cast<uint32_t>(i), WrapNode(nodes[i])).Check();
        }
        
        return array;
    }
	static void SetPoseCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
	{
		v8::Isolate* isolate = args.GetIsolate();
		
		// Get the CallbackData from the function's external data
		auto data = static_cast<CallbackData*>(
			v8::Local<v8::External>::Cast(args.Data())->Value()
		);
		
		if (args.Length() < 1) return;
		
		auto self = args.Holder();
		auto wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
		auto node = static_cast<SOMNode*>(wrap->Value());
		
		auto poseObj = args[0]->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
		node->setPose(data->worker->PoseFromObject(isolate,poseObj));
	}

	static void CreateNodeCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
	{
		v8::Isolate* isolate = args.GetIsolate();
		auto data = static_cast<CallbackData*>(
			v8::Local<v8::External>::Cast(args.Data())->Value()
		);
        // Check arguments
        if (args.Length() < 2) 
        {
            isolate->ThrowException(v8::String::NewFromUtf8(
                isolate, 
                "createNode requires parent and name parameters"
            ).ToLocalChecked());
            return;
        }
        
        // Get parent node
        if (!args[0]->IsObject()) 
        {
            isolate->ThrowException(v8::String::NewFromUtf8(
                isolate, 
                "First parameter must be a node object"
            ).ToLocalChecked());
            return;
        }
        auto context = isolate->GetCurrentContext();

        auto parentObj = args[0]->ToObject(context).ToLocalChecked();
        
        // Check if the parent is a valid node object
        if (!parentObj->InternalFieldCount() || parentObj->InternalFieldCount() < 1) 
        {
            isolate->ThrowException(v8::String::NewFromUtf8(
                isolate, 
                "Invalid parent node object"
            ).ToLocalChecked());
            return;
        }

        auto parentWrap = v8::Local<v8::External>::Cast(parentObj->GetInternalField(0));
        auto parentNode = static_cast<SOMNode*>(parentWrap->Value());
        
        // Get node name
        std::string nodeName = ToString(isolate, args[1]);
        
		auto newNode = data->scene->createNode(parentNode->shared_from_this(),nodeName);
		
		args.GetReturnValue().Set(data->worker->WrapNode(newNode));
	}
	static void GetNodeByUidCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
	{
		v8::Isolate* isolate = args.GetIsolate();
		auto data = static_cast<CallbackData*>(
			v8::Local<v8::External>::Cast(args.Data())->Value()
		);
		
		if (args.Length() < 1)
			return;
		
		uint64_t nodeUid = ToUid( args[0]);
		auto newNode = data->scene->getElementById(nodeUid);
		
		args.GetReturnValue().Set(data->worker->WrapNode(newNode));
	}
	
	static void GetNodesByNameCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
	{
		v8::Isolate* isolate = args.GetIsolate();
		v8::HandleScope handleScope(isolate);
		auto context = isolate->GetCurrentContext();
		
		// Get the CallbackData from the function's external data
		auto data = static_cast<CallbackData*>(
			v8::Local<v8::External>::Cast(args.Data())->Value()
		);
		
		// Check arguments
		if (args.Length() < 1) 
		{
			args.GetReturnValue().Set(v8::Array::New(isolate, 0));
			return;
		}
		
		// Get the name parameter
		std::string name = ToString(isolate, args[0]);
		
		// Search for nodes
		auto nodes = data->scene->getNodesByName(name);
		
		// Create JavaScript array for results
		auto result = v8::Array::New(isolate, static_cast<int>(nodes.size()));
		
		// Fill the array with wrapped nodes
		for (size_t i = 0; i < nodes.size(); ++i) 
		{
			result->Set(
				context,
				static_cast<uint32_t>(i),
				data->worker->WrapNode(nodes[i])
			).Check();
		}
		args.GetReturnValue().Set(result);
	}
	static void GetRootNodeCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
	{
		auto data = static_cast<CallbackData*>(
			v8::Local<v8::External>::Cast(args.Data())->Value()
		);
		auto newNode = data->scene->getRootNode();
		
		args.GetReturnValue().Set(data->worker->WrapNode(newNode));
	}
	V8Worker::CallbackData* callbackData=nullptr;
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
		
		// Create scene object
		scene = std::make_shared<Scene>();

		auto sceneTemplate = v8::ObjectTemplate::New(isolate);
		sceneTemplate->SetInternalFieldCount(1);

		// Add document to global object
		global->Set(
			v8::String::NewFromUtf8(isolate, "scene").ToLocalChecked(),
			sceneTemplate
		);

		// Make te C++ log function available in JavaScript
		global->Set(isolate, "log",v8::FunctionTemplate::New(isolate, LogCallback));

		// Create external data for callbacks
		callbackData = new CallbackData{this, scene};
		auto externalData = v8::External::New(isolate, callbackData);


		// Add scene methods with external data
		sceneTemplate->Set(
			isolate,
			"createNode",
			v8::FunctionTemplate::New(isolate, CreateNodeCallback, externalData)
		);
		sceneTemplate->Set(
			isolate,
			"getNodeByUid",
			v8::FunctionTemplate::New(isolate, GetNodeByUidCallback, externalData)
		);
		sceneTemplate->Set(
			isolate,
			"getNodesByName",
			v8::FunctionTemplate::New(isolate, GetNodesByNameCallback, externalData)
		);
		sceneTemplate->Set(
			isolate,
			"getRootNode",
			v8::FunctionTemplate::New(isolate, GetRootNodeCallback, externalData)
		);

		// Create node template for wrapper
		auto nodeTempl = v8::ObjectTemplate::New(isolate);
		nodeTempl->SetInternalFieldCount(1);

		// Add node methods with external data
		nodeTempl->Set(
			isolate,
			"setPose",
			v8::FunctionTemplate::New(isolate, SetPoseCallback, externalData)
		); 
		
		nodeTempl->SetAccessor(
            v8::String::NewFromUtf8(isolate, "children").ToLocalChecked(),
            GetChildrenCallback,
            nullptr,  // No setter
            externalData
        );

		// Store templates in the global object
		global->Set(
			isolate,
			"scene",
			sceneTemplate
		);

		global->Set(
			isolate,
			"SOMNode",
			nodeTempl
		);
        nodeTemplate.Reset(isolate, nodeTempl);

		// Create and store context
		v8::Local<v8::Context> ctx = v8::Context::New(isolate, nullptr, global);
		context.Reset(isolate, ctx);

		// Set up scene object in context
		v8::Context::Scope context_scope(ctx);
		v8::Local<v8::Object> sceneObj = ctx->Global()
			->Get(ctx, v8::String::NewFromUtf8(isolate, "scene").ToLocalChecked())
			.ToLocalChecked()
			->ToObject(ctx)
			.ToLocalChecked();
		sceneObj->SetInternalField(0, v8::External::New(isolate, scene.get()));

		// Create the root node.
		scene->createNode(scene,"root");
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
		if (DWORD bytesWritten;!WriteFile(pipeHandle, output.c_str(),
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
			if (!Write(result))
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
        nodeTemplate.Reset(); 
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

int main(int argc, char *argv[])
{
	const char * const *arguments=argv;
	if (argc < 3)
	{
		static const char *default_arg[3] = {argv[0], "1", "Test"};
		argc = 3;
		arguments = default_arg;
		// std::cerr << "Usage: v8worker.exe <tabId> <pipeName>" << std::endl;
		// return 1;
	}
#ifdef _MSC_VER
	if (argc >= 4)
	{
		bool pause_for_debugger = false;
		if (std::string(arguments[3]) == "WAIT_FOR_DEBUGGER")
			Sleep(20000);
	}
#endif
	try
	{
		std::string execName = arguments[0];
		std::string pipeName = arguments[2];
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

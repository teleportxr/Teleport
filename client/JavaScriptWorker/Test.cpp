#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <fstream>
#include <iterator>
#include "TeleportClient/V8ProcessManager.h"

#pragma optimize("", off)

auto read_file(std::string_view path) -> std::string {
    constexpr auto read_size = std::size_t{4096};
    auto stream = std::ifstream{path.data()};
    stream.exceptions(std::ios_base::badbit);
    
    auto out = std::string{};
    auto buf = std::string(read_size, '\0');
    while (stream.read(& buf[0], read_size)) {
        out.append(buf, 0, stream.gcount());
    }
    out.append(buf, 0, stream.gcount());
    return out;
}

int main(int argc, char *argv[])
{
	try
	{
		// Create and run worker
		V8ProcessManager processManager(false);
		
	/*	if(uint32_t tabId=1; processManager.InitializeTab(tabId))
		{
			while(!processManager.IsReady(tabId))
			{
				Sleep(1);
			}
			std::string script = "'Hello World\\n'";
			processManager.ExecuteScript(tabId, script);
		}
		else
		{
			std::cerr << "Error: Failed to launch tab." << std::endl;
		}*/
		if(uint32_t tabId=2; processManager.InitializeTab(tabId))
		{
			while(!processManager.IsReady(tabId))
			{
				Sleep(1);
			}
			std::ifstream script_ifs("test.js");
			std::string script(std::istreambuf_iterator<char>{script_ifs}, {});
			script_ifs.close();
			processManager.ExecuteScript(tabId, script);
		}
		else
		{
			std::cerr << "Error: Failed to launch tab." << std::endl;
		}
		Sleep(12000);
		return 0;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}
#include <string>
#include <stdint.h>
enum TeleportInterProcessCommand
{
	TELEPORT_COMMAND_LINE=1
};
extern bool EnsureSingleProcess(const std::string &cmdLine);
extern std::string GetExternalCommandLine(int64_t lParam);
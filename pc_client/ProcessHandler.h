#include <string>
enum TeleportInterProcessCommand
{
	TELEPORT_COMMAND_LINE=1
};
extern bool EnsureSingleProcess(const std::string &cmdLine);
extern std::string GetExternalCommandLine(__int64 lParam);
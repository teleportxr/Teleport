#pragma once

#include <string>
#include <vector>

struct ReplCommand
{
	std::string verb;
	std::vector<std::string> args;

	bool IsValid() const { return !verb.empty(); }
};

class ReplCommandParser
{
public:
	ReplCommandParser() = default;

	ReplCommand Parse(const std::string &line);

private:
	std::vector<std::string> Tokenize(const std::string &line);
};

#include "ReplCommandParser.h"
#include <sstream>
#include <algorithm>

ReplCommand ReplCommandParser::Parse(const std::string &line)
{
	auto tokens = Tokenize(line);
	if (tokens.empty())
		return ReplCommand{};

	ReplCommand cmd;
	cmd.verb = tokens[0];
	cmd.args.assign(tokens.begin() + 1, tokens.end());
	return cmd;
}

std::vector<std::string> ReplCommandParser::Tokenize(const std::string &line)
{
	std::vector<std::string> tokens;
	std::istringstream iss(line);
	std::string token;

	while (iss >> token)
	{
		tokens.push_back(token);
	}

	return tokens;
}

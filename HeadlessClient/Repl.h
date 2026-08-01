#pragma once

#include "HeadlessClient.h"
#include "ReplCommandParser.h"
#include <string>
#include <atomic>
#include <thread>

class Repl
{
public:
	Repl(HeadlessClient &client);
	~Repl();

	void Run();
	void Stop();
	bool IsStopping() const { return stopping.load(); }

private:
	void ProcessCommand(const ReplCommand &cmd);
	void PrintHelp() const;
	void PrintStatus() const;
	void PrintIdentity() const;

	HeadlessClient &client;
	ReplCommandParser parser;
	std::atomic<bool> stopping{false};
};

#pragma once

#include "HeadlessConnection.h"
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

//! Owns every live HeadlessConnection in the service, keyed by a small integer id
//! that control clients use to address connections. All public methods lock the
//! same mutex, which also serialises command-handler threads against the tick
//! thread: SessionClient is not internally thread-safe for concurrent calls, so
//! holding the lock for the duration of a tick or a command is required, not
//! incidental.
class ConnectionManager
{
public:
	ConnectionManager() = default;
	~ConnectionManager();

	//! Snapshot of one connection for the `connections` command.
	struct ConnectionInfo
	{
		uint32_t id;
		std::string url;
		bool connected;
		std::string statusLine; //! First line of HeadlessConnection::GetStatus().
	};

	//! Create a connection and start connecting. Returns the new id, or 0 on failure
	//! (error is filled in). The new id is monotonically increasing and never reused
	//! within the process lifetime.
	uint32_t Create(const std::string &url, std::string &error);

	//! Disconnect and destroy a connection. False if no such id.
	bool Destroy(uint32_t id);

	//! Disconnect everything; called during service shutdown.
	void DestroyAll();

	//! Fetch a connection by id (nullptr if none). The caller must hold no lock of
	//! its own; the returned pointer is only safe to use inside WithConnection()
	//! or a method that holds the manager lock — use WithConnection().
	template <typename F>
	auto WithConnection(uint32_t id, F &&fn) -> decltype(fn(std::declval<HeadlessConnection &>()))
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto it = connections.find(id);
		if (it == connections.end())
			throw std::out_of_range("no such connection");
		return fn(*it->second);
	}

	bool Exists(uint32_t id) const;

	//! Tick every connection; called from the service's fixed-rate main loop.
	void TickAll(double time, double dt);

	std::vector<ConnectionInfo> List() const;

private:
	mutable std::mutex mutex;
	std::map<uint32_t, std::unique_ptr<HeadlessConnection>> connections;
	uint32_t nextId = 1;
};

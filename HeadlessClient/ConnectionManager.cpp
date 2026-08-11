#include "ConnectionManager.h"

ConnectionManager::~ConnectionManager()
{
	DestroyAll();
}

uint32_t ConnectionManager::Create(const std::string &url, std::string &error)
{
	std::lock_guard<std::mutex> lock(mutex);
	auto conn = std::make_unique<HeadlessConnection>();
	if (!conn->Connect(url))
	{
		error = "could not initiate connection to " + url;
		return 0;
	}
	uint32_t id = nextId++;
	connections.emplace(id, std::move(conn));
	return id;
}

bool ConnectionManager::Destroy(uint32_t id)
{
	std::lock_guard<std::mutex> lock(mutex);
	auto it = connections.find(id);
	if (it == connections.end())
		return false;
	// ~HeadlessConnection() calls Disconnect(); do it explicitly so a failed
	// disconnect can't propagate from a destructor.
	it->second->Disconnect();
	connections.erase(it);
	return true;
}

void ConnectionManager::DestroyAll()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (auto &[id, conn] : connections)
		conn->Disconnect();
	connections.clear();
}

bool ConnectionManager::Exists(uint32_t id) const
{
	std::lock_guard<std::mutex> lock(mutex);
	return connections.find(id) != connections.end();
}

void ConnectionManager::TickAll(double time, double dt)
{
	std::lock_guard<std::mutex> lock(mutex);
	for (auto &[id, conn] : connections)
		conn->TickOnce(time, dt);
}

std::vector<ConnectionManager::ConnectionInfo> ConnectionManager::List() const
{
	std::lock_guard<std::mutex> lock(mutex);
	std::vector<ConnectionInfo> out;
	out.reserve(connections.size());
	for (const auto &[id, conn] : connections)
	{
		ConnectionInfo info;
		info.id = id;
		info.url = conn->GetUrl();
		info.connected = conn->IsConnected();
		std::string status = conn->GetStatus();
		size_t nl = status.find('\n');
		info.statusLine = (nl == std::string::npos) ? status : status.substr(0, nl);
		out.push_back(std::move(info));
	}
	return out;
}

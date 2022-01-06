// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <string>
#include <mutex>
#include <platform.hpp>

namespace avs
{

class LibraryLoader
{
public:
	explicit LibraryLoader(const std::string& name);
	~LibraryLoader();

	LibraryLoader(const LibraryLoader&) = delete;
	LibraryLoader(LibraryLoader&&) = delete;

	LibraryHandle load();
	void unload();

	const std::string& getName() const
	{
		return m_name;
	}

private:
	const std::string m_name;
	LibraryHandle m_handle;
	std::mutex m_mutex;
	unsigned int m_refcount;
};

class ScopedLibraryHandle
{
public:
	ScopedLibraryHandle(LibraryLoader& loader)
		: m_loader(loader)
	{
		m_handle = m_loader.load();
	}
	~ScopedLibraryHandle()
	{
		if(m_handle)
{
			m_loader.unload();
		}
	}
	operator LibraryHandle() const
	{
		return m_handle;
	}
	LibraryHandle take()
	{
		const LibraryHandle handle = m_handle;
		m_handle = nullptr;
		return handle;
	}

private:
	LibraryLoader& m_loader;
	LibraryHandle m_handle;
};

} // avs
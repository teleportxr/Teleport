// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <platform.hpp>
#include <mutex>

namespace avs
{
	/*!
	 * Utility for making HTTP/HTTPS requests.
	 */
	class AVSTREAM_API HTTPUtil
	{
	public:
		HTTPUtil();
		~HTTPUtil();

	private:
		mutable std::mutex m_mutex;
	};
} 
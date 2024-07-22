// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include "abi_p.hpp"
#include <libavstream/context.hpp>

namespace avs
{
	struct Context::Private
	{
		AVSTREAM_PRIVATEINTERFACE_BASE(Context)

		void log(LogSeverity severity, const char* msg) const;
	
		static Context* m_self;
		MessageHandlerFunc m_messageHandler;
		void* m_userData;
	};

} // avs
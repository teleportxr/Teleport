// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <iostream>
#include <context_p.hpp>

using namespace avs;

Context* Context::Private::m_self = nullptr;

static void defaultMessageHandler(LogSeverity severity, const char* msg, void*)
{
//	static const char* const severityNames[] = {
//		"Debug", "Info", "Warning", "Error", "Critical"
//	};
	if (severity == LogSeverity::Info || severity == LogSeverity::Debug)
		std::cout << msg;
	else
		std::cerr << msg;
	//std::cerr << "libavstream: " << severityNames[static_cast<int>(severity)] << ": " << msg << "\n";
}

Context::Context()
	: m_d(new Context::Private(this))
{
	assert(!Private::m_self);
	Private::m_self = this;
	d().m_messageHandler = defaultMessageHandler;
}

Context::~Context()
{
	delete m_d;
	Private::m_self = nullptr;
}

Context* Context::instance()
{
	assert(Private::m_self);
	return Private::m_self;
}

void Context::setMessageHandler(MessageHandlerFunc handler, void* userData)
{
	if (handler)
	{
		d().m_messageHandler = handler;
		d().m_userData = userData;
	}
	else
	{
		d().m_messageHandler = defaultMessageHandler;
	}
}

void Context::Private::log(LogSeverity severity, const char* msg) const
{
	if (m_messageHandler)
	{
		m_messageHandler(severity, msg, m_userData);
	}
}

void Context::log(LogSeverity severity, const char* msg) const
{
	d().log(severity,msg);
}

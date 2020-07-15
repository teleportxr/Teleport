// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>

namespace avs
{
	/*!
	 * Message handler function prototype.
	 *
	 * \param severity Message severity class.
	 * \param msg Null-terminated string containing the message.
	 * \param userData Custom user data pointer.
	 */
	typedef void(*MessageHandlerFunc)(LogSeverity severity, const char* msg, void* userData);

	/*!
	 * Global library context.
	 *
	 * Context object managing application-global state.
	 * \note A single instance of this class must be instantiated by the application before accessing any other library functionality.
	 */
	class AVSTREAM_API Context final
	{
		AVSTREAM_PUBLICINTERFACE_FINAL(Context)
	public:
		Context();
		~Context();

		Context(const Context&) = delete;
		Context(Context&&) = delete;

		/*! Get instance of the global context object. */
		static Context* instance();

		/*!
		 * Set custom message handler.
		 * \note Default (library provided) message handler prints to standard output.
		 * \param handler Message handler function; if nullptr, default message handler is restored.
		 * \param userData Pointer to user supplied data passed to message handler function (never dereferenced, may be nullptr).
		 */
		void setMessageHandler(MessageHandlerFunc handler, void* userData);
	};

} // avs
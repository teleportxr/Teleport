#pragma once
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <streambuf>
#include <string>

// Tees std::cout/std::cerr to the original console streams and (optionally) a log file.
// Mirrors the public API of Windows' VisualStudioDebugOutput so pc_client/Main.cpp can
// use the same code path on both platforms.
class DebugOutput
{
	class TeeStreambuf : public std::streambuf
	{
	public:
		TeeStreambuf(std::streambuf *console, std::ofstream &file, bool &to_file, bool &to_console, std::mutex &mtx)
			: console_buf(console), log_file(file), to_logfile(to_file), to_console_window(to_console), out_mutex(mtx)
		{
		}

	protected:
		int overflow(int c) override
		{
			if (c == traits_type::eof())
				return traits_type::not_eof(c);
			std::lock_guard<std::mutex> lock(out_mutex);
			char ch = traits_type::to_char_type(c);
			if (to_console_window && console_buf)
				console_buf->sputc(ch);
			if (to_logfile && log_file.good())
				log_file.put(ch);
			return c;
		}

		std::streamsize xsputn(const char *s, std::streamsize n) override
		{
			std::lock_guard<std::mutex> lock(out_mutex);
			if (to_console_window && console_buf)
				console_buf->sputn(s, n);
			if (to_logfile && log_file.good())
				log_file.write(s, n);
			return n;
		}

		int sync() override
		{
			std::lock_guard<std::mutex> lock(out_mutex);
			int rc = 0;
			if (to_console_window && console_buf)
				rc |= console_buf->pubsync();
			if (to_logfile && log_file.good())
				log_file.flush();
			return rc;
		}

	private:
		std::streambuf *console_buf;
		std::ofstream &log_file;
		bool &to_logfile;
		bool &to_console_window;
		std::mutex &out_mutex;
	};

public:
	DebugOutput(bool send_to_console = true, const char *logfilename = nullptr, size_t /*bufsize*/ = 16)
		: to_logfile(false), to_console_window(send_to_console),
		  old_cout_buffer(std::cout.rdbuf()), old_cerr_buffer(std::cerr.rdbuf()),
		  out_tee(old_cout_buffer, logFile, to_logfile, to_console_window, out_mutex),
		  err_tee(old_cerr_buffer, logFile, to_logfile, to_console_window, out_mutex)
	{
		std::cout.rdbuf(&out_tee);
		std::cerr.rdbuf(&err_tee);
		if (logfilename)
			setLogFile(logfilename);
	}

	virtual ~DebugOutput()
	{
		std::cout.flush();
		std::cerr.flush();
		std::cout.rdbuf(old_cout_buffer);
		std::cerr.rdbuf(old_cerr_buffer);
		if (to_logfile)
		{
			logFile << std::endl;
			logFile.close();
		}
	}

	void setLogFile(const char *logfilename)
	{
		if (!logfilename || !*logfilename)
			return;
		if (logFile.is_open())
			logFile.close();
		logFile.open(logfilename, std::ios::out | std::ios::trunc);
		if (logFile.good())
		{
			to_logfile = true;
		}
		else
		{
			to_logfile = false;
			// Use the original cerr buffer directly so we don't recurse into the tee.
			if (old_cerr_buffer)
			{
				std::string msg = std::string("Failed to create logfile ") + logfilename + "\n";
				old_cerr_buffer->sputn(msg.data(), static_cast<std::streamsize>(msg.size()));
			}
		}
	}

protected:
	std::ofstream logFile;
	bool to_logfile;
	bool to_console_window;
	std::streambuf *old_cout_buffer;
	std::streambuf *old_cerr_buffer;
	std::mutex out_mutex;
	TeeStreambuf out_tee;
	TeeStreambuf err_tee;
};

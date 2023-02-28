#pragma once
#define OutputDebugString
#define _getcwd
#ifndef _MAX_PATH
#define _MAX_PATH 500
#endif
#include <fstream>
#include <iostream>
#include <sstream>
#include <time.h>
#include <cerrno>

#define __stdcall
typedef void (__stdcall *DebugOutputCallback)(const char *);
class DebugOutput;
// A simple delegate, it will usually be a function partially bound with std::bind.
typedef std::function<void(const std::string &str)> WriteStringDelegate;

class vsBufferedStringStreamBuf : public std::streambuf
{
	WriteStringDelegate writeStringDelegate;
public:
	vsBufferedStringStreamBuf(int bufferSize,WriteStringDelegate d) :writeStringDelegate(d)
	{
		if (bufferSize)
		{
			char *ptr = new char[bufferSize];
			setp(ptr, ptr + bufferSize);
		}
		else
			setp(0, 0);
	}
	virtual ~vsBufferedStringStreamBuf() 
	{
		//sync();
		delete[] pbase();
	}
private:
	int	overflow(int c)
	{
		std::lock_guard<std::mutex> logLock(logMutex);
		sync();
		if (c != EOF)
		{
			if (pbase() == epptr())
			{
				std::string temp;
				temp += char(c);
				writeStringDelegate(temp);
			}
			else
				sputc((char)c);
		}
		return 0;
	}

	int	sync()
	{
		if (pbase() != pptr())
		{
			int len = int(pptr() - pbase());
			std::string temp(pbase(), len);
			writeStringDelegate(temp);
			setp(pbase(), epptr());
		}
		return 0;
	}
protected:
	std::mutex logMutex;
};

class DebugOutput 
{
	vsBufferedStringStreamBuf out;
	vsBufferedStringStreamBuf err;
public:
    DebugOutput(bool send_to_output_window=true,
							const char *logfilename=NULL,size_t bufsize=(size_t)16
							,DebugOutputCallback c=NULL
							,DebugOutputCallback e=NULL)
							:out((int)bufsize,std::bind(&DebugOutput::writeOutputString,this,std::placeholders::_1))
							,err((int)bufsize,std::bind(&DebugOutput::writeErrorString,this,std::placeholders::_1))
		,to_logfile(false)
		,old_cout_buffer(NULL)
		,old_cerr_buffer(NULL)
		,outputCallback(c)
		,errorCallback(e)
	{
		if(c&&!e)
			errorCallback=c;
		if(e&&!c)
			outputCallback=e;
		to_output_window=send_to_output_window;
		if(logfilename)
			setLogFile(logfilename);
		old_cout_buffer=std::cout.rdbuf(&out);
		old_cerr_buffer=std::cerr.rdbuf(&err);
	}
	virtual ~DebugOutput()
	{
		if(to_logfile)
		{
			logFile<<std::endl;
			logFile.close();
		}
		std::cout.rdbuf(old_cout_buffer);
		std::cerr.rdbuf(old_cerr_buffer);
	}
	void setLogFile(const char *logfilename)
	{
		std::string fn=logfilename;
		if(fn.find(":")>=fn.length())
		{
#ifndef _XBOX_ONE
			char buffer[_MAX_PATH];
			if(_getcwd(buffer,_MAX_PATH))
			{
				fn=buffer;
			}
#endif
			fn+="/";
			fn+=logfilename;
		}
		logFile.open(fn.c_str());
		if(logFile.good())
			to_logfile=true;
		else if(errno!=0)
		{
			std::cerr<<"Failed to create logfile "<<fn.c_str()<<std::endl;
			errno=0;
		}
	}
	void setOutputCallback(DebugOutputCallback c)
	{
		outputCallback=c;
	}
	void setErrorCallback(DebugOutputCallback c)
	{
		errorCallback=c;
	}
    void writeErrorString(const std::string &str)
    {
		writeString(str,true);
    }
    void writeOutputString(const std::string &str)
    {
		writeString(str,false);
    }
    void setToOutputWindow(bool o)
	{
		to_output_window=o;
	}
    void writeString(const std::string &str,bool error)
    {
		if(to_logfile)
			logFile<<str.c_str();
		if(error&&errorCallback)
		{
			errorCallback(str.c_str());
		}
		else if(!error&&outputCallback)
		{
			outputCallback(str.c_str());
		}
		if(to_output_window)
		{
#ifdef UNICODE
			std::wstring wstr(str.length(), L' '); // Make room for characters
			// Copy string to wstring.
			std::copy(str.begin(), str.end(), wstr.begin());
			OutputDebugString(wstr.c_str());
#else
	        OutputDebugString(str.c_str());
#endif
		}
    }
protected:
	std::ofstream logFile;
	bool to_output_window;
	bool to_logfile;
	std::streambuf *old_cout_buffer;
	std::streambuf *old_cerr_buffer;
	DebugOutputCallback outputCallback;
	DebugOutputCallback errorCallback;
};
#include "Log.h"
#include <iostream>
#include <stdarg.h>
#ifdef __ANDROID__
#include <android/log.h>
class AndroidStreambuf : public std::streambuf
{
public:
	enum
	{
		bufsize=128
	}; // ... or some other suitable buffer size
	android_LogPriority logPriority;
	AndroidStreambuf(android_LogPriority p=ANDROID_LOG_INFO)
	{
		logPriority=p;
		this->setp(buffer, buffer+bufsize-1);
	}

private:
	int overflow(int c)
	{
		if(c==traits_type::eof())
		{
			*this->pptr()=traits_type::to_char_type(c);
			this->sbumpc();
		}
		return this->sync() ? traits_type::eof() : traits_type::not_eof(c);
	}
	std::string str;
	int sync()
	{
		int rc=0;
		if(this->pbase()!=this->pptr())
		{
			char writebuf[bufsize+1];
			memcpy(writebuf, this->pbase(), this->pptr()-this->pbase());
			writebuf[this->pptr()-this->pbase()]='\0';
			str+=writebuf;
			for(size_t pos=0;pos<str.length();pos++)
			{
				if(str[pos]=='\n')
				{
					str[pos]=0;
					rc=__android_log_write(logPriority, "TeleportVR", str.c_str())>0;
					if(str.length()>pos+1)
						str=str.substr(pos+1,str.length()-pos-1);
					else
						str.clear();
				}
			}
			this->setp(buffer, buffer+bufsize-1);
		}
		return rc;
	}

	char buffer[bufsize];
};

AndroidStreambuf androidCout;
AndroidStreambuf androidCerr(ANDROID_LOG_WARN);

void RedirectStdCoutCerr()
{
	auto *oldout = std::cout.rdbuf(&androidCout);
	auto *olderr = std::cerr.rdbuf(&androidCerr);
	if (oldout != &androidCout)
	{
		__android_log_write(ANDROID_LOG_DEBUG, "TeleportVR", "redirected cout");
	}
	std::cout<<"Testing cout redirect."<<std::endl;
	if (olderr != &androidCerr)
	{
		__android_log_write(ANDROID_LOG_DEBUG, "TeleportVR", "redirected cerr");
	}
	std::cerr<<"Testing cerr redirect."<<std::endl;
}
#endif

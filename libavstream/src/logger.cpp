

#include "logger.hpp"
#include <sstream>
#include <string.h>
#include <iostream>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define AVSLOG(Severity) std::cerr<<__FILENAME__<<"("<<__LINE__<<"): "<<#Severity<<": "
#define AVSLOGONCE(Severity) 
#define AVSLOG_NOSPAM(Severity) 

using namespace avs;
Logger &Logger::GetInstance()
{
    static Logger l(LogSeverity::Debug);
    return l;
}
 Logger::Logger(LogSeverity severity)
	: m_severity(severity), queue_(1000)
{
#if QUEUE_LOGGING
	loggingThread = new std::thread([this]() {
		processLogQueue();
	  });
#endif
}
void Logger::processLogQueue() noexcept
{
    while (running_)
    {
        for (auto next = queue_.getNextToRead(); queue_.size() && next; next = queue_.getNextToRead())
        {
            std::cout << next->str;
            queue_.updateReadIndex();
        }
    }
    using namespace std::literals::chrono_literals;
    std::this_thread::yield();
}
// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <string>
#include <sstream>
#include "context_p.hpp"
#include <string.h>
#include <iostream>
#include <thread>
#include <fmt/core.h>

#define QUEUE_LOGGING 0
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define AVSLOG(Severity) std::cerr<<__FILENAME__<<"("<<__LINE__<<"): "<<#Severity<<": "
#define AVSLOGONCE(Severity,txt,...) 
#define AVSLOG_NOSPAM(Severity,txt,...) 
namespace avs
{
  template<typename T>
  class LFQueue final {
  public:
    LFQueue(std::size_t num_elems) :
        store_(num_elems, T()) /* pre-allocation of vector storage. */ {
    }

    T *getNextToWriteTo() noexcept {
      return &store_[next_write_index_];
    }

    void updateWriteIndex() noexcept {
        if(num_elements_<store_.size())
        {
            next_write_index_ = (next_write_index_ + 1) % store_.size();
            num_elements_++;
      }
    }

    auto getNextToRead() const noexcept -> const T * {
      return (size() ? &store_[next_read_index_] : nullptr);
    }

    auto updateReadIndex() noexcept {
      next_read_index_ = (next_read_index_ + 1) % store_.size();
	  assert(num_elements_ != 0);
      num_elements_--;
    }

    auto size() const noexcept {
      return num_elements_.load();
    }

    // Deleted default, copy & move constructors and assignment-operators.
    LFQueue() = delete;

    LFQueue(const LFQueue &) = delete;

    LFQueue(const LFQueue &&) = delete;

    LFQueue &operator=(const LFQueue &) = delete;

    LFQueue &operator=(const LFQueue &&) = delete;

  private:
    std::vector<T> store_;

    std::atomic<size_t> next_write_index_ = {0};
    std::atomic<size_t> next_read_index_ = {0};

    std::atomic<size_t> num_elements_ = {0};
  };
    struct LogChunk
    {
        char str[200];
    };
	class Logger
	{
        std::atomic<bool> running_ = {true};
        LFQueue<LogChunk> queue_;
		std::thread *loggingThread=nullptr;
		void processLogQueue() noexcept;
    
	public:
        static Logger &GetInstance();
		explicit Logger(LogSeverity severity);

        auto log(const std::string &s) noexcept {
            auto *t=(queue_.getNextToWriteTo());
            if(t){
                size_t len=s.size()+1;
                if(len>199)
                    len=199;
                memcpy(t->str,s.data(),len);
                t->str[199]=0;
                queue_.updateWriteIndex();
          }
        }

        auto warn(const std::string &s) noexcept {
          log(s);
        }

		template<typename T>
		Logger& operator<<(T arg)
		{
			if (m_severity == LogSeverity::Never)
				return *this;
			std::ostringstream stream;
			stream << arg;
			Context::instance()->d().log(m_severity, stream.str().c_str());
			return *this;
		}

	private:
		const LogSeverity m_severity;
	};
    
	template <typename... Args>
	void Log(Logger &l,const char *txt, Args... args)
	{
		std::string str = fmt::format(txt,  args...);
		l.log(str);
	}
    
	template <typename... Args>
	void Info(Logger &l,const char *file, int line, const char *function,const char *txt, Args... args)
	{
		std::string str = fmt::format("{0} ({1}): info: {2}: {3}", file, line,function, txt);
		str = fmt::format(str,  args...);
		l.log(str);
	}
    
	template <typename... Args>
	void Warn(Logger &l,const char *file, int line, const char *function,const char *txt, Args... args)
	{
		std::string str = fmt::format("{0} ({1}): info: {2}: {3}", file, line,function, txt);
		str = fmt::format(str,  args...);
		l.warn(str);
	}

} // avs

#define AVS_LOG(txt, ...) \
    avs::Info(logger,__FILE__, __LINE__,__func__,txt,##__VA_ARGS__)
#define AVS_LOG_SIMPLE(txt, ...)\
    avs::Log(logger,txt,##__VA_ARGS__)
#define AVS_WARN(txt, ...) \
    avs::Warn(logger,__FILE__, __LINE__,__func__,txt,##__VA_ARGS__)

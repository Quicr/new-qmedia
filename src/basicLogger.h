#pragma once
#include <mutex>
#include <transport/logger.h>

namespace qmedia
{

class basicLogger : public qtransport::LogHandler
{
public:
    void log(qtransport::LogLevel level, const std::string& string) override;

private:
    std::mutex mutex;
};

};        // namespace qmedia

// TODO(trigaux): Getting ready for swapping to some fancier logging lib.
// clang-format off
#define LOG(logger, level, msg) do { std::ostringstream os; os << msg; logger.log(level, os.str()); } while(0)
#define LOG_FATAL(logger, msg) LOG(logger, qtransport::LogLevel::fatal, msg)
#define LOG_CRITICAL(logger, msg) LOG(logger, qtransport::LogLevel::fatal, msg)
#define LOG_ERROR(logger, msg) LOG(logger, qtransport::LogLevel::error, msg)
#define LOG_WARNING(logger, msg)  LOG(logger, qtransport::LogLevel::warn, msg)
#define LOG_INFO(logger, msg)  LOG(logger, qtransport::LogLevel::info, msg)
#define LOG_DEBUG(logger, msg) LOG(logger, qtransport::LogLevel::debug, msg)
// clang-format on

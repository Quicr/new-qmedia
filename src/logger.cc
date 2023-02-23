
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#include <iostream>
#include <iomanip>
#include <algorithm>
#include "qmedia/logger.hh"

Logger::Logger(bool output_to_console) :
    Logger("", "", nullptr, output_to_console)
{
}

Logger::Logger(const std::string &process_name, bool output_to_console) :
    Logger(process_name, "", nullptr, output_to_console)
{
}

Logger::Logger(const std::string &process_name,
               const std::string &component_name,
               bool output_to_console) :
    Logger(process_name, component_name, nullptr, output_to_console)
{
}

Logger::Logger(const std::string &component_name,
               const LoggerPointer &parent_logger,
               bool output_to_console) :
    Logger("", component_name, parent_logger, output_to_console)
{
}

Logger::Logger(const std::string &process_name,
               const std::string &component_name,
               const LoggerPointer &parent_logger,
               bool output_to_console) :
    process_name(process_name),
    component_name(component_name),
    parent_logger(parent_logger),
    log_facility(LogFacility::CONSOLE),
    log_level(parent_logger ? parent_logger->GetLogLevel() : LogLevel::INFO),
    info_buf(this, LogLevel::INFO),
    warning_buf(this, LogLevel::WARNING),
    error_buf(this, LogLevel::ERROR),
    critical_buf(this, LogLevel::CRITICAL),
    debug_buf(this, LogLevel::DEBUG),
    console_buf(this, LogLevel::INFO, true),
    output_to_console(output_to_console),
    info(&info_buf),
    warning(&warning_buf),
    error(&error_buf),
    critical(&critical_buf),
    debug(&debug_buf),
    console(&console_buf)
{
}

Logger::~Logger()
{
    // Only the root logger deals with actual facilities
    if (!parent_logger)
    {
        // Close the syslog if it is open
        if (log_facility == LogFacility::SYSLOG) closelog();

        // Close the open log file
        if (log_facility == LogFacility::FILE) log_file.close();
    }
}

void Logger::Log(LogLevel level, const std::string &message, bool console)
{
    std::string formatted_message;        // Formatted message to log

    console = console || output_to_console;

    // If not logging, return. Ask the parent about the facility facility
    // since only the root knows the actual facility.
    if (GetLogFacility() == LogFacility::NONE) return;

    // Simply return the log level of the message is higher what's to be logged
    if (level > log_level) return;

    // Prepare the formatted message to produce
    if (component_name.length() > 0)
    {
        formatted_message = "[" + component_name + "] " + message;
    }
    else
    {
        formatted_message = message;
    }

    // If there is a parent logger, send the logging message to it
    if (parent_logger)
    {
        parent_logger->Log(level, formatted_message, console);
    }
    else
    {
        // Only the root logger object calls this block of code

        // Log to the terminal or syslog as appropriate
        if (log_facility == LogFacility::SYSLOG)
        {
            syslog(MapLogLevel(level), "%s", formatted_message.c_str());
        }

        if ((log_facility != LogFacility::SYSLOG) || (console))
        {
            // Get the current time in human-readable form
            std::string timestamp = GetTimestamp();

            // Lock the mutex to ensure only one thread is writing
            logger_mutex.lock();

            try
            {
                // Update the formatted message to include the log level
                formatted_message = timestamp + " [" + LogLevelString(level) +
                                    "] " + formatted_message;

                // Output the log message to the appropriate facility
                if (log_facility == LogFacility::FILE)
                {
                    log_file << formatted_message << std::endl;
                }

                if ((log_facility == LogFacility::CONSOLE) || (console))
                {
                    std::cout << formatted_message << std::endl;
                }

                if (log_facility == LogFacility::NOTIFY)
                {
                    log_callback(level, formatted_message);
                }
            }
            catch (...)
            {
                // Unlock the mutex
                logger_mutex.unlock();

                // Re-throw the exception
                throw;
            }

            // Unlock the mutex
            logger_mutex.unlock();
        }
    }
}

void Logger::Log(const std::string &message)
{
    Log(LogLevel::INFO, message);
}

void Logger::SetLogFacility(LogFacility facility, std::string filename)
{
    // Just return if this is a child Logger object
    if (parent_logger) return;

    // Make a change only if the facility changed
    if (log_facility != facility)
    {
        // Stop logging to syslog if we were
        if (log_facility == LogFacility::SYSLOG) closelog();

        // Stop logging to a file if we were
        if (log_facility == LogFacility::FILE) log_file.close();

        // Open syslog if appropriate
        if (facility == LogFacility::SYSLOG)
        {
#ifdef _WIN32
            error << "SYSLOG not supported on Windows" << std::flush;
#else
            openlog(process_name.c_str(), LOG_PID, LOG_DAEMON);
#endif
        }

        // Open logging file if appropriate
        if (facility == LogFacility::FILE)
        {
            log_file.open(filename, std::ios::out | std::ios::app);
            if (!log_file.is_open())
            {
                std::cerr << "ERROR: Logger unable to open log file for "
                             "writing: "
                          << filename << std::endl;
                facility = LogFacility::NONE;
            }
        }

        // Set the logging facility
        log_facility = facility;
    }
}

LogFacility Logger::GetLogFacility()
{
    if (parent_logger) return parent_logger->GetLogFacility();

    return log_facility;
}

void Logger::SetLogLevel(LogLevel level)
{
    log_level = level;
}

void Logger::SetLogLevel(const std::string level)
{
    std::string level_comparator = level;

    // Convert the level string to uppercase
    std::transform(
        level.begin(), level.end(), level_comparator.begin(), ::toupper);

    try
    {
        // Map from the log level string to LogLevel value
        log_level = log_level_map.at(level_comparator);
    }
    catch (const std::out_of_range &e)
    {
        log_level = LogLevel::INFO;
        Log(LogLevel::ERROR, "Unknown log level: " + level, true);
    }
}

LogLevel Logger::GetLogLevel()
{
    return log_level;
}

bool Logger::IsDebugging()
{
    return log_level >= LogLevel::DEBUG;
}

void Logger::SetLogCallback(LogCallback log_callback)
{
    this->log_callback = log_callback;
}

int Logger::MapLogLevel(LogLevel level) const
{
#ifdef _WIN32
    return 0;
#else
    int priority;

    switch (level)
    {
        case LogLevel::CRITICAL:
            priority = LOG_CRIT;
            break;

        case LogLevel::ERROR:
            priority = LOG_ERR;
            break;

        case LogLevel::WARNING:
            priority = LOG_WARNING;
            break;

        case LogLevel::INFO:
            priority = LOG_INFO;
            break;

        case LogLevel::DEBUG:
            priority = LOG_DEBUG;
            break;

        default:
            priority = LOG_INFO;
            break;
    }

    return priority;
#endif
}

std::string Logger::LogLevelString(LogLevel level) const
{
    std::string log_level_string = "INFO";

    // Determine log level string for logging
    for (auto &item : log_level_map)
    {
        if (item.second == level)
        {
            log_level_string = item.first;
            break;
        }
    }

    return log_level_string;
}

std::string Logger::GetTimestamp() const
{
    std::ostringstream oss;

    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_result
    {
    };
#ifdef _WIN32
    localtime_s(&tm_result, &t);
#else
    localtime_r(&t, &tm_result);
#endif
    oss << std::put_time(&tm_result, "%FT%T") << "." << std::setfill('0')
        << std::setw(3) << (now_ms.time_since_epoch().count()) % 1000;

    return oss.str();
}

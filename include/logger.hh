
#pragma once

#include <string>
#include <sstream>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <unordered_map>
#include "syslog_interface.hh"

#undef ERROR

// Define log level enumeration
enum class LogLevel
{
    CRITICAL,
    ERROR,
    WARNING,
    INFO,
    DEBUG
};

// Define the logging facility enumeration
enum class LogFacility
{
    NONE,
    CONSOLE,
    SYSLOG,
    FILE,
    NOTIFY
};

/*
 * Define a shared pointer type that can be used to pass pointers to
 * logging objects into "slave" logging objects.
 */
class Logger;
typedef std::shared_ptr<Logger> LoggerPointer;
typedef std::function<void(LogLevel level, const std::string &message)>
    LogCallback;

// Logger object declaration
class Logger : protected SyslogInterface
{
protected:
    // Stream buffer used to capture log messages
    class LoggingBuf : public std::stringbuf
    {
    public:
        LoggingBuf(Logger *logger, LogLevel log_level, bool console = false) :
            std::stringbuf(),
            logger(logger),
            log_level(log_level),
            console(console),
            busy(false)
        {
        }

    protected:
        Logger *logger;
        LogLevel log_level;
        bool console;
        std::atomic<bool> busy;
        std::thread::id owning_thread;
        std::mutex buffer_mutex;
        std::condition_variable signal;

        void thread_sync()
        {
            bool wait_result;

            std::unique_lock<std::mutex> lock(buffer_mutex);
            wait_result = signal.wait_for(
                lock, std::chrono::seconds(1), [&]() -> bool {
                    return (!busy || (busy && owning_thread ==
                                                  std::this_thread::get_id()));
                });

            if (!wait_result)
            {
                logger->Log(LogLevel::ERROR,
                            "Somebody forgot to call std::flush!?");
                busy = false;
            }

            if (!busy)
            {
                busy = true;
                owning_thread = std::this_thread::get_id();
            }
        }

        virtual std::streamsize xsputn(const char *c, std::streamsize n)
        {
            // Control thread access to the string buffer
            thread_sync();
            return std::stringbuf::xsputn(c, n);
        }

        virtual int sync()
        {
            // Control thread access to the string buffer
            thread_sync();

            logger->Log(log_level, str(), console);
            str("");

            // We're done with the buffer now
            busy = false;

            // Let another thread take control
            signal.notify_one();

            return 0;
        }
    };

    // Mapping of log level strings to LogLevel values
    std::unordered_map<std::string, LogLevel> log_level_map = {
        {"CRITICAL", LogLevel::CRITICAL},
        {"ERROR", LogLevel::ERROR},
        {"WARNING", LogLevel::WARNING},
        {"INFO", LogLevel::INFO},
        {"DEBUG", LogLevel::DEBUG}};

public:
    // Constructor
    Logger(bool output_to_console = false);

    // Constructor with process name
    Logger(const std::string &process_name, bool output_to_console = false);

    // Constructor with process name and component name
    Logger(const std::string &process_name,
           const std::string &component_name,
           bool output_to_console = false);

    // Constructor for child Logger objects
    Logger(const std::string &component_name,
           const LoggerPointer &parent_logger,
           bool output_to_console = false);

    // Disallow the copy constructor, as this could be a problem for
    // creation of certain log types (e.g., logging to files)
    Logger(const Logger &) = delete;

    // Destructor
    virtual ~Logger();

    // Function to log messages
    void Log(LogLevel level, const std::string &message, bool console = false);

    // Function to log messages using LogLevel::INFO
    void Log(const std::string &message);

    // Set the logging facility
    void SetLogFacility(LogFacility facility, std::string filename = {});

    // What is the current logging facility?
    LogFacility GetLogFacility();

    // Set the log level to be output (default is LogLevel::INFO)
    void SetLogLevel(LogLevel level);

    // Set the log level to be output (default is "INFO")
    void SetLogLevel(const std::string level = "INFO");

    // Get the current log level
    LogLevel GetLogLevel();

    // Check to see if debug messages are to be logged
    bool IsDebugging();

    // Set log message callback.
    void SetLogCallback(LogCallback log_callback);

protected:
    // Constructor called by other constructors
    Logger(const std::string &process_name,
           const std::string &component_name,
           const LoggerPointer &parent_logger,
           bool output_to_console = false);

    int MapLogLevel(LogLevel level) const;        // Map log level to syslog
                                                  // level
    std::string LogLevelString(LogLevel level) const;        // Log level string
    std::string GetTimestamp() const;        // Return current timestamp

    std::string process_name;                     // Program name for logging
    std::string component_name;                   // Component name for logging
    LoggerPointer parent_logger;                  // Parent logging object
    std::atomic<LogFacility> log_facility;        // Facility to which to log
    std::atomic<LogLevel> log_level;              // Log level to be logged

    // buffers used with streaming interface
    LoggingBuf info_buf;
    LoggingBuf warning_buf;
    LoggingBuf error_buf;
    LoggingBuf critical_buf;
    LoggingBuf debug_buf;
    LoggingBuf console_buf;

    std::mutex logger_mutex;         // Mutex to synchronize logging
    std::ofstream log_file;          // Stream used for file logging
    bool output_to_console;          // flag to force output to console
    LogCallback log_callback;        // Callback for log messages

public:
    // Streaming interfaces
    std::ostream info;
    std::ostream warning;
    std::ostream error;
    std::ostream critical;
    std::ostream debug;
    std::ostream console;
};




#pragma once

#ifndef _WIN32
#include <syslog.h>
#endif

// Syslog interface declaration
class SyslogInterface
{
public:
    virtual ~SyslogInterface() {}

    virtual void openlog(const char* ident, int option, int facility)
    {
#ifndef _WIN32
        ::openlog(ident, option, facility);
#endif
    }

    virtual void closelog(void)
    {
#ifndef _WIN32
        ::closelog();
#endif
    }

    // Note: C++ will now allow the following to be a virtual function
    template <typename... Args>
    void syslog(int priority, const char* format, Args... args)
    {
#ifndef _WIN32
        ::syslog(priority, format, args...);
#endif
    }
};

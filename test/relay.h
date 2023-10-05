#pragma once

#include <quicr/quicr_server.h>

class LocalhostRelay
{
public:
    LocalhostRelay(uint16_t port, const std::string& cert, const std::string& key);

    void run() const;
    void stop();

private:
    std::shared_ptr<quicr::Server> server;
};

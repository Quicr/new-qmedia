#pragma once

#include <quicr/quicr_server.h>

class LocalhostRelay
{
public:
    static constexpr auto port = uint16_t(12345);
    static constexpr auto cert_file = "server-cert.pem";
    static constexpr auto key_file = "server-key.pem";

    LocalhostRelay();

    void run() const;
    void stop();

private:
    std::shared_ptr<quicr::Server> server;
};


#pragma once

#include <string>
#include <thread>
#include <queue>
#include <cstdint>
#include <mutex>
#include <cassert>
#include <sframe/sframe.h>

#include "transport.hh"
#include "packet.hh"

namespace neo_media
{
class TransportManager;
///
/// TransportManager
///
class NetTransportUDP : public NetTransport
{
    /* roles and repsobilites of this class :
       - encrypts and decrypts things
       - sends and revies ack / nacks
       - takes cares of congestion controll
       - hides encode / decode of packet into protobuf, RTP whatever
     */
public:
    NetTransportUDP(TransportManager *,
                    std::string sfuName_in,
                    uint16_t sfuPort_in);        // create a Client
    NetTransportUDP(TransportManager *, uint16_t sfuPort);        // create a
                                                                  // server
    virtual ~NetTransportUDP();

    virtual bool ready();
    virtual void close();
    virtual void shutdown();
    virtual bool doSends();
    virtual bool doRecvs();
    virtual NetTransport::PeerConnectionInfo getConnectionInfo()
    {
        return PeerConnectionInfo{sfuAddr, sfuAddrLen, {}};
    }

    bool isServer() { return m_isServer; }

    bool read(NetTransport::Data &packet);
    bool write(const NetTransport::Data &packet);

    const bool m_isServer;
    TransportManager *transportManager;
    int fd;
    std::string sfuName;
    uint16_t sfuPort;
    struct sockaddr_storage sfuAddr;
    socklen_t sfuAddrLen;
};

}        // namespace neo_media

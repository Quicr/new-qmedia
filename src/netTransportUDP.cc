#include <string.h>        // memcpy
#include <iostream>
#include <thread>
#include <cassert>

#if defined(__linux) || defined(__APPLE__)
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#if defined(__linux__)
#include <net/ethernet.h>
#include <netpacket/packet.h>
#elif defined(__APPLE__)
#include <net/if_dl.h>
#elif defined(_WIN32)
#include <ws2tcpip.h>
#include <winsock2.h>
#endif

#include "transport_manager.hh"
#include "netTransportUDP.hh"

using namespace neo_media;

NetTransportUDP::~NetTransportUDP()
{
    close();
}

bool NetTransportUDP::ready()
{
    return (fd > 0);
}

void NetTransportUDP::shutdown()
{
#if defined(_WIN32)
    closesocket(fd);
#else
    ::shutdown(fd, SHUT_RDWR);
#endif
};

void NetTransportUDP::close()
{
    if (fd > 0)
    {
#if defined(_WIN32)
        closesocket(fd);
#else
        ::close(fd);
#endif
    }

    fd = 0;
}

bool NetTransportUDP::doSends()
{
    std::string buffer;
    buffer.reserve(1500);
    struct sockaddr_storage remoteAddr;
    socklen_t remoteAddrLen;
    PeerConnectionInfo peer_info;

    bool ok = transportManager->getDataToSendToNet(
        buffer, &peer_info, &remoteAddrLen);
    if ((!ok) || (buffer.size() == 0))
    {
        return false;
    }

    int numSent = sendto(fd,
                         buffer.data(),
                         buffer.size(),
                         0 /*flags*/,
                         (struct sockaddr *) &peer_info.addr,
                         sizeof(sockaddr_in));
    if (numSent < 0)
    {
#if defined(_WIN32)
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT)
        {
            return false;
        }
        else
        {
            std::cerr << "sending on UDP socket got error: "
                      << WSAGetLastError() << std::endl;
            assert(0);
        }
#else
        // TODO: this drops packet on floor, we need a way to
        // requeue/resend
        int e = errno;
        std::cerr << "sending on UDP socket got error: " << strerror(e)
                  << std::endl;
        assert(0);        // TODO
#endif
    }
    else if (numSent != (int) buffer.size())
    {
        assert(0);        // TODO
    }

    return true;
}

bool NetTransportUDP::write(const NetTransport::Data &packet)
{
    int numSent = sendto(fd,
                         packet.data.data(),
                         packet.data.size(),
                         0 /*flags*/,
                         (struct sockaddr *) &packet.peer.addr,
                         sizeof(sockaddr_in));
    if (numSent < 0)
    {
#if defined(_WIN32)
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT)
        {
            return false;
        }
        else
        {
            std::cerr << "sending on UDP socket got error: "
                      << WSAGetLastError() << std::endl;
            assert(0);
        }
#else
        // TODO: this drops packet on floor, we need a way to
        // requeue/resend
        int e = errno;
        std::cerr << "sending on UDP socket got error: " << strerror(e)
                  << std::endl;
        assert(0);        // TODO
#endif
    }
    else if (numSent != (int) packet.data.size())
    {
        assert(0);        // TODO
    }

    return true;
}

bool NetTransportUDP::doRecvs()
{
    const int dataSize = 1500;
    std::string buffer(dataSize, 0);

    struct sockaddr_storage remoteAddr;
    memset(&remoteAddr, 0, sizeof(remoteAddr));
    socklen_t remoteAddrLen = sizeof(remoteAddr);

    int rLen = recvfrom(fd,
                        buffer.data(),
                        buffer.size(),
                        0 /*flags*/,
                        (struct sockaddr *) &remoteAddr,
                        &remoteAddrLen);
    if (rLen < 0)
    {
#if defined(_WIN32)
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT || error == WSAECONNRESET)
        {
            return false;
        }
        else
        {
            std::cerr << "reading from UDP socket got error: "
                      << WSAGetLastError() << std::endl;
            assert(0);
        }
#else
        int e = errno;
        if (e == EAGAIN)
        {
            // timeout on read
            return false;
        }
        else
        {
            std::cerr << "reading from UDP socket got error: " << strerror(e)
                      << std::endl;
            assert(0);        // TODO
        }
#endif
    }

    if (rLen == 0)
    {
        return false;
    }
    buffer.resize(rLen);
    auto peer_info = NetTransport::PeerConnectionInfo{
        remoteAddr, remoteAddrLen, {}};

    transportManager->recvDataFromNet(buffer, peer_info);

    return true;
}

// TODO: normalize the API
bool NetTransportUDP::read(NetTransport::Data &packet)
{
    const int dataSize = 1500;
    std::string buffer;
    buffer.resize(dataSize);
    struct sockaddr_storage remoteAddr;
    memset(&remoteAddr, 0, sizeof(remoteAddr));
    socklen_t remoteAddrLen = sizeof(remoteAddr);
    int rLen = recvfrom(fd,
                        buffer.data(),
                        buffer.size(),
                        0 /*flags*/,
                        (struct sockaddr *) &remoteAddr,
                        &remoteAddrLen);
    if (rLen < 0)
    {
#if defined(_WIN32)
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT)
        {
            return false;
        }
        else
        {
            std::cerr << "reading from UDP socket got error: "
                      << WSAGetLastError() << std::endl;
            assert(0);
        }
#else
        int e = errno;
        if (e == EAGAIN)
        {
            // timeout on read
            return false;
        }
        else
        {
            std::cerr << "reading from UDP socket got error: " << strerror(e)
                      << std::endl;
            assert(0);        // TODO
        }
#endif
    }

    if (rLen == 0)
    {
        return false;
    }

    buffer.resize(rLen);
    packet.data = std::move(buffer);
    packet.peer.addrLen = remoteAddrLen;
    memcpy(&(packet.peer.addr), &remoteAddr, remoteAddrLen);
    return true;
}

NetTransportUDP::NetTransportUDP(TransportManager *t,
                                 std::string sfuName_in,
                                 uint16_t sfuPort_in) :
    m_isServer(false),
    transportManager(t),
    sfuName(std::move(sfuName_in)),
    sfuPort(sfuPort_in)
{
    // create a Client
#if defined(_WIN32)
    WSADATA wsaData;
    int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_err)
    {
        assert(0);
    }
#endif

    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == -1)
    {
        assert(0);        // TODO
    }

    // make socket non blocking IO
    struct timeval timeOut;
    timeOut.tv_sec = 0;
    timeOut.tv_usec = 2000;        // 2 ms
    int err = 0;
    /*int err = setsockopt(
        fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeOut, sizeof(timeOut));
    if (err)
    {
        assert(0);        // TODO
    }*/

    struct sockaddr_in srvAddr;
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srvAddr.sin_port = 0;
    err = bind(fd, (struct sockaddr *) &srvAddr, sizeof(srvAddr));
    if (err)
    {
        assert(0);
    }

    std::string sPort = std::to_string(htons(sfuPort));
    struct addrinfo hints = {}, *address_list = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    err = getaddrinfo(sfuName.c_str(), sPort.c_str(), &hints, &address_list);
    if (err)
    {
        assert(0);
    }
    struct addrinfo *item = NULL, *found_addr = NULL;
    for (item = address_list; item != NULL; item = item->ai_next)
    {
        if (item->ai_family == AF_INET && item->ai_socktype == SOCK_DGRAM &&
            item->ai_protocol == IPPROTO_UDP)
        {
            found_addr = item;
            break;
        }
    }
    if (found_addr == NULL)
    {
        assert(0);
    }

    // TODO: make it work for IPv6
    struct sockaddr_in *ipv4 = (struct sockaddr_in *) &sfuAddr;
    memcpy(ipv4, found_addr->ai_addr, found_addr->ai_addrlen);
    ipv4->sin_port = htons(sfuPort);
    sfuAddrLen = sizeof(sfuAddr);
}

///
/// Server TransportManager
///

NetTransportUDP::NetTransportUDP(TransportManager *t, uint16_t sfuPort) :
    // create a server
    m_isServer(true),
    transportManager(t)
{
#if defined(_WIN32)
    WSADATA wsaData;
    int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_err)
    {
        assert(0);
    }
#endif

    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
    {
        assert(0);        // TODO
    }

    // set for re-use
    int one = 1;
    int err = setsockopt(
        fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &one, sizeof(one));
    if (err != 0)
    {
        assert(0);        // TODO
    }

    // make socket non blocking IO
    struct timeval timeOut;
    timeOut.tv_sec = 0;
    timeOut.tv_usec = 2000;        // 2 ms
    err = setsockopt(
        fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeOut, sizeof(timeOut));
    if (err)
    {
        assert(0);        // TODO
    }

    struct sockaddr_in srvAddr;
    memset((char *) &srvAddr, 0, sizeof(srvAddr));
    srvAddr.sin_port = htons(sfuPort);
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    err = bind(fd, (struct sockaddr *) &srvAddr, sizeof(srvAddr));
    if (err < 0)
    {
        assert(0);        // TODO
    }

    std::cout << "UdpSocket: port " << sfuPort << ", fd " << fd << std::endl;
}

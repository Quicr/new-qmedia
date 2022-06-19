#include <string.h>        // memcpy
#include <iostream>
#include <thread>
#include <cassert>
#include <ostream>
#include <iomanip>
#include <sstream>

#include "transport_manager.hh"
#include "netTransportUDP.hh"

namespace neo_media
{
static std::string to_hex(const std::vector<uint8_t> &data)
{
    std::stringstream hex(std::ios_base::out);
    hex.flags(std::ios::hex);
    for (const auto &byte : data)
    {
        hex << std::setw(2) << std::setfill('0') << int(byte);
    }
    return hex.str();
}

TransportManager::TransportManager(NetTransport::Type type,
                                   const std::string &sfuName_in,
                                   int sfuPort_in,
                                   Metrics::MetricsPtr metricsPtr,
                                   const LoggerPointer &parent_logger) :
    netTransport(
        make_transport(type, this, sfuName_in, sfuPort_in, parent_logger)),
    metrics(metricsPtr)
{
    logger = std::make_shared<Logger>("TransportManager", parent_logger);
}

TransportManager::~TransportManager()
{
    shutDown = true;        // tell threads to stop
    recv_cv.notify_all();
    send_cv.notify_all();

    netTransport->shutdown();

    if (metrics)
    {
        logger->info << "TransportMgr pushing metrics" << std::flush;
        metrics->push();
    }

    if (recvThread.joinable())
    {
        recvThread.join();
    }

    if (sendThread.joinable())
    {
        sendThread.join();
    }

    // TODO - free mem in Q's
}

bool TransportManager::empty()
{
    std::lock_guard<std::mutex> lock(recvQMutex);

    return recvQ.empty();
}

void TransportManager::waitForPacket()
{
    std::unique_lock<std::mutex> ulock(recvQMutex);
    recv_cv.wait(ulock, [&]() -> bool { return (shutDown || !recvQ.empty()); });
    ulock.unlock();
}

PacketPointer TransportManager::recv()
{
    PacketPointer ret = nullptr;
    {
        std::lock_guard<std::mutex> lock(recvQMutex);
        if (!recvQ.empty())
        {
            ret = std::move(recvQ.front());
            recvQ.pop();
        }
    }

    if (!ret)
    {
        return ret;
    }

    switch (ret->packetType)
    {
        case Packet::Type::StreamContent:
            // std::cout << "media_packet: cnx_id:"
            //          << to_hex(ret->peer_info.transport_connection_id)
            //          << std::endl;
            // send ack TODO: wire it through retrans module once we have it
            if (!isLoopback)
            {
#if 0
                PacketPointer content_ack = std::make_unique<Packet>();
                content_ack->packetType = Packet::Type::StreamContentAck;
                content_ack->transportSequenceNumber =
                    ret->transportSequenceNumber;
                memcpy(&content_ack->peer_info.addr,
                       &(ret->peer_info.addr),
                       ret->peer_info.addrLen);
                content_ack->peer_info.transport_connection_id =
                    ret->peer_info.transport_connection_id;
                content_ack->peer_info.addrLen = ret->peer_info.addrLen;
                content_ack->clientID = ret->clientID;
                send(std::move(content_ack));
#endif
            }

            break;
        case Packet::Type::StreamContentAck:
            // Send to transmission manager module
            // txManager->ackHandle(ret->aggregateSequenceNumber);
            break;
            break;
        default:
            logger->debug << "Unsupported packet_type" << std::endl;
    }
    return ret;
}

void TransportManager::runNetRecv()
{
    while (!shutDown)
    {
        bool gotData = netTransport->doRecvs();

        if (!gotData)
        {
            //   std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void TransportManager::runNetSend()
{
    while (!shutDown)
    {
        std::unique_lock<std::mutex> ulock(sendQMutex);
        send_cv.wait(ulock,
                     [&]() -> bool { return (shutDown || !sendQ.empty()); });
        ulock.unlock();
        /*
                if (sendQ.empty())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
        */
        if (!netTransport->doSends())
        {
            // TODO accumulate count and report metric
        }
    }
}

///
/// Client TransportManager
///

// Used for testing only
ClientTransportManager::ClientTransportManager() :
    TransportManager(NetTransport::Type::UDP, "localhost", -1, nullptr, nullptr),
    senderId(sframe::MLSContext::SenderID(0x0000)),
    mls_context(sframe::CipherSuite::AES_GCM_128_SHA256, 8),
    current_epoch(0)
{
    rtx_mgr = std::make_unique<RtxManager>(false, this, nullptr);
}

ClientTransportManager::ClientTransportManager(
    NetTransport::Type type,
    std::string sfuName_in,
    uint16_t sfuPort_in,
    Metrics::MetricsPtr metricsPtr,
    const LoggerPointer &parent_logger) :
    TransportManager(type, sfuName_in, sfuPort_in, metricsPtr, parent_logger),
    sfuName(std::move(sfuName_in)),
    sfuPort(sfuPort_in),
    senderId(sframe::MLSContext::SenderID(0x1234)),
    mls_context(sframe::CipherSuite::AES_GCM_128_SHA256, 8),
    current_epoch(0)
{
    if (type == NetTransport::Type::UDP)
    {
        // quic/quicr have their own rtx mechanisms, hence enable rtx
        // just for udp transport.
        rtx_mgr = std::make_unique<RtxManager>(true, this, metricsPtr);
    }
}

void ClientTransportManager::start()
{
    recvThread = std::thread(recvThreadFunc, this);
    sendThread = std::thread(sendThreadFunc, this);
}

ClientTransportManager::~ClientTransportManager() = default;

void ClientTransportManager::send(PacketPointer packet)
{
    assert(packet);
    auto conn_info = netTransport->getConnectionInfo();
    memcpy(&(packet->peer_info.addr), &(conn_info.addr), conn_info.addrLen);
    packet->peer_info.addrLen = conn_info.addrLen;
    // Set the packet transport sequence number
    if (packet->packetType != Packet::Type::StreamContentAck)
    {
        packet->transportSequenceNumber = nextTransportSeq;
    }
    nextTransportSeq++;

    if (!netEncode(packet.get(), packet->encoded_data))
    {
        // TODO
        logger->error << "Packet encoding failed" << std::flush;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(sendQMutex);
        sendQ.push(std::move(packet));
        // TODO - check Q not too deep
    }
    // Notify that a packet is ready to send
    send_cv.notify_all();
}

size_t TransportManager::hasDataToSendToNet()
{
    std::lock_guard<std::mutex> lock(sendQMutex);
    return sendQ.empty() ? 0 : sendQ.front()->encoded_data.size();
}

void ClientTransportManager::loopback(PacketPointer packet)
{
    isLoopback = true;
    assert(packet);
    {
        std::lock_guard<std::mutex> lock(recvQMutex);
        recvQ.push(std::move(packet));
    }
    recv_cv.notify_all();
}

void ClientTransportManager::setCryptoKey(uint64_t epoch,
                                          const bytes &epoch_secret)
{
    current_epoch = epoch;
    mls_context.add_epoch(epoch, epoch_secret);
}

bytes ClientTransportManager::protect(const bytes &plaintext)
{
    auto ct_out = bytes(plaintext.size() + sframe::max_overhead);
    auto ct = mls_context.protect(current_epoch, senderId, ct_out, plaintext);
    return bytes(ct.begin(), ct.end());
}

bytes ClientTransportManager::unprotect(const bytes &ciphertext)
{
    auto pt_out = bytes(ciphertext.size());
    auto pt = mls_context.unprotect(pt_out, ciphertext);
    return bytes(pt.begin(), pt.end());
}

///
/// Server TransportManager
///

ServerTransportManager::ServerTransportManager(NetTransport::Type type,
                                               int sfuPort,
                                               Metrics::MetricsPtr metricsPtr,
                                               const LoggerPointer &logger) :
    TransportManager(type, "", sfuPort, metricsPtr, logger)
{
    assert(netTransport);
    recvThread = std::thread(recvThreadFunc, this);
    sendThread = std::thread(sendThreadFunc, this);
}

ServerTransportManager::~ServerTransportManager()
{
}

void ServerTransportManager::send(PacketPointer packet)
{
    assert(packet);

    // assert(packet->remoteAddrLen > 0);

    {
        std::lock_guard<std::mutex> lock(sendQMutex);
        sendQ.push(std::move(packet));
        // TODO - check Q not too deep
    }
    // Notify that a packet is ready to send
    send_cv.notify_all();
}

// TODO: refactor
void TransportManager::recordMetric(MeasurementType mtype,
                                    const PacketPointer &packetPointer)
{
    // TODO: metrics sould batch instead
    static const auto measurement_name = std::map<MeasurementType, std::string>{
        {MeasurementType::PacketRate_Tx, "TxPacketCount"},
        {MeasurementType::PacketRate_Rx, "RxPacketCount"},
        {MeasurementType::FrameRate_Tx, "TxFrameCount"},
        {MeasurementType::FrameRate_Rx, "RxFrameCount"},
        {MeasurementType::QDepth_Tx, "TxQueueDepth"},
        {MeasurementType::QDepth_Rx, "RxQueueDepth"},
    };

    if (!metrics)
    {
        return;
    }

    // common tags
    auto tags = Metrics::Measurement::Tags{
        {"clientID", packetPointer->clientID},
        {"sourceID", packetPointer->sourceID}};

    // fields common for most of the metrics
    auto fields = Metrics::Measurement::Fields{{"count", 1}};

    // These are created by default
    if (!measurements.count(MeasurementType::QDepth_Tx))
    {
        measurements[MeasurementType::QDepth_Tx] = metrics->createMeasurement(
            measurement_name.at(MeasurementType::QDepth_Tx), tags);
    }

    if (!measurements.count(MeasurementType::QDepth_Rx))
    {
        measurements[MeasurementType::QDepth_Rx] = metrics->createMeasurement(
            measurement_name.at(MeasurementType::QDepth_Rx), tags);
    }

    auto now = std::chrono::system_clock::now();

    switch (mtype)
    {
        case MeasurementType::PacketRate_Tx:
        case MeasurementType::PacketRate_Rx:
        {
            if (!measurements.count(mtype))
            {
                measurements[mtype] = metrics->createMeasurement(
                    measurement_name.at(mtype), {});
            }
            tags.push_back({"media_type", (uint64_t) packetPointer->mediaType});
            fields.push_back({"packet_size", packetPointer->data.size()});
            auto entry = Metrics::Measurement::TimeEntry{std::move(tags),
                                                         std::move(fields)};
            measurements[mtype]->set_time_entry(now, std::move(entry));
            num_metrics_accumulated++;
        }
        break;
        default:
            break;
    }

    // record queue depth regardless
    measurements[MeasurementType::QDepth_Tx]->set(
        now, Metrics::Measurement::Field{"depth", sendQ.size()});
    measurements[MeasurementType::QDepth_Rx]->set(
        now, Metrics::Measurement::Field{"depth", recvQ.size()});

    if (num_metrics_accumulated > NUM_METRICS_TO_ACCUMULATE)
    {
        metrics->push();
        num_metrics_accumulated = 0;
    }
}

bool TransportManager::netEncode(Packet *packet, std::string &data_out)
{
    if (Packet::Type::StreamContent == packet->packetType &&
        packet->data.empty())
    {
        return false;
    }

    auto result = Packet::encode(packet, data_out);
    if (!result)
    {
        // TODO: may be log metric
        return false;
    }

    return true;
}

bool TransportManager::netDecode(const std::string &data_in, Packet *packet_out)
{
    assert(data_in.size() > 0);

    auto result = Packet::decode(data_in, packet_out);
    if (!result)
    {
        return false;
    }

    packet_out->authTag.resize(1);
    packet_out->authTag[0] = 0xF;
    return true;
}

bool TransportManager::recvDataFromNet(
    std::string &data_in,
    NetTransport::PeerConnectionInfo peer_info)
{
    logger->debug << "recvDataFromNet <<<" << std::endl;
    PacketPointer packet = std::make_unique<Packet>();

    packet->peer_info.addrLen = peer_info.addrLen;
    memcpy(&(packet->peer_info.addr), &peer_info.addr, peer_info.addrLen);
    if (!peer_info.transport_connection_id.empty())
    {
        std::copy(
            peer_info.transport_connection_id.begin(),
            peer_info.transport_connection_id.end(),
            std::back_inserter(packet->peer_info.transport_connection_id));
    }

    if (!netDecode(data_in, packet.get()))
    {
        logger->error << "Packet decoding failed" << std::flush;
        return false;
    }

    logger->info << "[R]: Type:" << packet->packetType << "," << packet->encodedSequenceNum << std::flush;
#if 0
    // decrypt if its client transportManager
    if (Type::Client == type() && !packet->data.empty())  {
        auto* client = dynamic_cast<ClientTransportManager *>(this);

        try {
            auto pt = client->unprotect(packet->data);
            packet->encrypted = false;
            packet->data = std::move(pt);
        } catch (std::exception& e) {
            logger->error << "Packet decryption failed" << std::flush;
            return false;
        }
    }
#endif
    if (packet->packetType == Packet::Type::StreamContent)
    {
        recordMetric(MeasurementType::PacketRate_Rx, packet);
    }
    // let rtx_manager know about the packet
    if (rtx_mgr && packet->packetType == Packet::Type::StreamContentAck)
    {
        rtx_mgr->ackHandle(packet, std::chrono::steady_clock::now());
    }

    {
        std::lock_guard<std::mutex> lock(recvQMutex);
        recvQ.push(std::move(packet));
        // TODO - check Q not too deep
    }
    // Notify the client of data availability
    recv_cv.notify_all();
    return true;
}

bool TransportManager::getDataToSendToNet(NetTransport::Data& data) {
    // get packet to send from Q
    PacketPointer packet = nullptr;
    {
        std::lock_guard<std::mutex> lock(sendQMutex);
        if (sendQ.empty())
        {
            return false;
        }
        packet = std::move(sendQ.front());
        sendQ.pop();
    }

    assert(packet);

    if (!packet->peer_info.transport_connection_id.empty())
    {
        auto &cnx_id = packet->peer_info.transport_connection_id;
        std::copy(cnx_id.begin(),
                  cnx_id.end(),
                  std::back_inserter(data.peer.transport_connection_id));
    }

    data.source_id = packet->sourceID;
    data.peer.addrLen = packet->peer_info.addrLen;

    memcpy(&data.peer.addr,
           &(packet->peer_info.addr),
           packet->peer_info.addrLen);

    if (packet->mediaType == Packet::MediaType::AV1)
    {
        logger->info << "[S]: SeqNo " <<  packet->transportSequenceNumber
                     << " video_frame_type: " << (int) packet->videoFrameType
                     << std::flush;
    }
    data.data = std::move(packet->encoded_data);

    return true;
}

bool TransportManager::getDataToSendToNet(
    std::string &data_out,
    NetTransport::PeerConnectionInfo *peer_info,
    socklen_t *addrLen)
{
    // get packet to send from Q
    PacketPointer packet = nullptr;
    {
        std::lock_guard<std::mutex> lock(sendQMutex);
        if (sendQ.empty())
        {
            return false;
        }
        packet = std::move(sendQ.front());
        sendQ.pop();
    }
    assert(packet);

    if (Packet::Type::StreamContent == packet->packetType)
    {
        assert(!packet->data.empty());
    }
#if 0
    // TODO - move encryption to when added to Q
    // TODO - checking data empty is not a fool proof and extensible way
    // Need to add type semantics on the packet
    if (Type::Client == type() && !packet->data.empty())
    {
        auto *client = dynamic_cast<ClientTransportManager *>(this);
        try
        {
            auto ct = client->protect(packet->data);
            packet->encrypted = true;
            packet->data = std::move(ct);
        } catch (std::exception &e) {
          logger->error << "Packet encryption failed" << std::flush;
          packet = nullptr;
          return false;
        }
   }  // encryption
#endif

    if (!packet->peer_info.transport_connection_id.empty())
    {
        auto &cnx_id = packet->peer_info.transport_connection_id;
        std::copy(cnx_id.begin(),
                  cnx_id.end(),
                  std::back_inserter(peer_info->transport_connection_id));
    }

    peer_info->addrLen = packet->peer_info.addrLen;
    memcpy(
        &peer_info->addr, &(packet->peer_info.addr), packet->peer_info.addrLen);
    *addrLen = peer_info->addrLen;
    logger->info << "[S]: Type:" << packet->packetType << ", " << packet->encodedSequenceNum << std::flush;

    data_out = std::move(packet->encoded_data);

    // pass the packet to rtx manager
    // For now only pass audio packets till we figure out the issue with video
    // pacing and frame assembley
    if (rtx_mgr && (packet->packetType == Packet::Type::StreamContent) &&
        (packet->mediaType == Packet::MediaType::Opus))
    {
        rtx_mgr->packHandle(std::move(packet),
                            std::chrono::steady_clock::now());
    }

    return true;
}

}        // namespace neo_media

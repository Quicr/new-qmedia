
#include <string.h>
#include <iostream>
#include <thread>
#include <cassert>
#include <stdlib.h>

#include "rtx_manager.hh"
#include "transport_manager.hh"

namespace neo_media
{
// TODO:  replace trans with callback
RtxManager::RtxManager(bool retx,
                       ClientTransportManager *trans,
                       Metrics::MetricsPtr metricsPtr)
{
    retx_enabled = retx;
    transport = trans;
    metrics = metricsPtr;
}

RtxManager::~RtxManager()
{
    shutdown = true;        // tell threads to stop
    signal.notify_all();

    if (timer_thread.joinable())
    {
        timer_thread.join();
    }
}

// Is retransmission enabled or not
bool RtxManager::isRetxEnabled()
{
    return retx_enabled;
}

// Packet Handler:
// Add a new transmitted packet to the retx list, update aux data and start
// timer thread if needed
bool RtxManager::packHandle(
    PacketPointer packet,
    std::chrono::time_point<std::chrono::steady_clock> now)
{
    // Check packet type
    if (packet->packetType != Packet::Type::Join &&
        packet->packetType != Packet::Type::StreamContent)
    {
        // log an eror
        std::clog << "RtxManager:packHandler: wrong packet type"
                  << packet->packetType;
        return false;
    }

    if (self_client_id == 0)
    {
        self_client_id = packet->clientID;
    }

    // Create a new aux structure
    AuxPacket aux;
    aux.ack_status = false;
    aux.send_time = now;

    // First lock the map and list
    std::lock_guard<std::mutex> lock(retx_mutex);

    // This is a retransmitted packet: find the original packet in the retx
    // list, append the packet pointer, get its aux entry in the map and update
    // its retx_seq_list Update the retransmitted packet aux and added it to the
    // aux map

    if (packet->retransmitted)
    {
        // TODO: Add a metric to trace how many times we scan the list. Optimize
        // this if needed
        for (auto &item : tx_list)
        {
            if (item.mediaType == packet->mediaType &&
                item.encodedSequenceNum == packet->encodedSequenceNum &&
                item.chunkFragmentNum == packet->chunkFragmentNum)
            {
                // Original packet found...extract the aux data
                auto &original_aux = aux_map[item.origTransportSeq];
                // add the retx packet sequence number to the original list of
                // sequence numbers
                original_aux.retx_seq_list.push_back(
                    packet->transportSequenceNumber);
                // add the original packet sequence number to the retx aux
                // packet list of sequence numbers
                aux.retx_seq_list.push_back(item.origTransportSeq);
                aux.retx = true;
                // Code review
                // aux_map.emplace(packet->aggreateSeqenceNumber,
                // AuxPacket{......});
                aux_map.insert({packet->transportSequenceNumber, aux});
                // replace packetpointer and make it valid
                item.packet = std::move(packet);
                item.validPointer = true;
                break;
            }
        }
    }
    else
    {
        // Packet is new not a retransmission...add it to the transmission list
        // and the aux map
        aux.retx = false;
        // Code review: Do emplace....two constructors for AuxPacket
        aux_map.insert({packet->transportSequenceNumber, aux});
        // create a transmit packet and add it to the tx list
        // should use emplace
        TransmitPacket p;
        p.mediaType = packet->mediaType;
        p.encodedSequenceNum = packet->encodedSequenceNum;
        p.chunkFragmentNum = packet->chunkFragmentNum;
        p.origTransportSeq = packet->transportSequenceNumber;
        p.validPointer = true;
        p.packet = std::move(packet);
        tx_list.push_back(std::move(p));
    }

    // Create a timer thread if it does not exist and retransmission is on
    // Code review: check if te thread is joinable can be used
    if (isRetxEnabled() && !timer_thread.joinable())
    {
        timer_thread = std::thread(&RtxManager::reTransmitter, this);
    }
    return true;
}

// Ack Handler:
// This could be an ack for original packet or a retransmission
// Find the aux data for the acked packet and the original packet and make them
// both acked as we will never retransmit again!
bool RtxManager::ackHandle(
    const PacketPointer &packet,
    std::chrono::time_point<std::chrono::steady_clock> now)
{
    // Check packet type
    if (packet->packetType != Packet::Type::StreamContentAck)
    {
        std::clog << "TransmitManager:ackHandler: wrong packet type: "
                  << packet->packetType;
        return false;
    }

    // Lock the aux map
    std::lock_guard<std::mutex> lock(retx_mutex);

    // Does the packet exist in aux map
    auto aux = aux_map.find(packet->transportSequenceNumber);
    if (aux != aux_map.end())
    {
        updateRTT(packet->transportSequenceNumber, aux->second.send_time, now);
        recordMetric(MeasurementType::RTT_Smooth, packet);
        // TODO: replace 1.5 rtt_retransmit_delay_constant_etc
        // TODO: make a sensible variable to avoid early retransmission that are
        // not necessary
        rtx_delay = std::chrono::milliseconds{
            (long long) (2 * rtt.smooth)};        // will be used for next timer
        if (rtx_delay < kInitialRtxDelay) rtx_delay = kInitialRtxDelay;
        aux->second.ack_status = true;

        // This is an original packet
        if (aux->second.retx == false)
        {
            // Ack  received for packet after retranmsmission?
            if (aux->second.retx_seq_list.empty() == false)
            {
                // std::clog << "Retx: ack received for orig after
                // retransmission. Seq:" << packet->transportSequenceNumber <<
                // "\n";
                spurious_rtx_count++;
            }
        }
        else
        {
            // ack for one of the retransmitted packet,
            // retrieve the orig seq_no ( there wll be only one entry)
            auto orig_seq = aux->second.retx_seq_list.front();

            // If original already acked then this is a spurious ack
            if (aux_map[orig_seq].ack_status)
            {
                // std::clog << "Retx: ack received after retransmission. Seq:"
                // << packet->transportSequenceNumber << "\n";
                spurious_rtx_count++;
            }
            else
            {
                // indicate original packet to also be acked
                aux_map[orig_seq].ack_status = true;
            }
        }
    }
    else
    {
        // Packet is not in the aux map...ack came late
        late_ack_count++;
    }

    return true;
}

bool RtxManager::nackHandle(
    PacketPointer packet,
    std::chrono::time_point<std::chrono::steady_clock> now)
{
    return false;
}

void RtxManager::updateRTT(
    std::uint64_t seq,
    std::chrono::time_point<std::chrono::steady_clock> tx_time,
    std::chrono::time_point<std::chrono::steady_clock> ack_rx_time)
{
    // TODO: Need to reset the measurements after persistent congestion or new
    // path Once congestion control feeds into this
    auto current_rtt = std::chrono::duration_cast<std::chrono::milliseconds>(
                           ack_rx_time - tx_time)
                           .count();
    if (current_rtt <= 0)
    {
        std::clog << "TransmitManager:calculateRTT: negative RTT value:packet "
                     "seq:"
                  << seq << "currentRTT:" << current_rtt << std::endl;
        return;
    }

    // For first sample....initialize numbers
    if (rtt.minimum == 0)
    {
        rtt.minimum = current_rtt;
        rtt.smooth = current_rtt;
        rtt.variance = current_rtt / 2;
    }
    // Subsequent samples...calculate numbers
    else
    {
        if (current_rtt < (long long) rtt.minimum) rtt.minimum = current_rtt;

        rtt.smooth = (current_rtt / kWeightRTt) +
                     (rtt.smooth * ((kWeightRTt - 1) / kWeightRTt));
        rtt.variance = (3 / 4 * rtt.variance) +
                       (1 / 4 * std::abs((int) (rtt.smooth - current_rtt)));
    }
}

void RtxManager::reTransmitter()
{
    auto current_timer = rtx_delay;

    // Lock the list and aux...the lock is automaticall released on wait and
    // reacquired after wait
    std::unique_lock<std::mutex> lock(retx_mutex);

    // loop waiting for retransmit timer
    while (true)
    {
        // Wait until shutdown or timer expires
        signal.wait_for(lock, current_timer, [&]() { return shutdown; });
        // Were we told to shutdown
        if (shutdown)
        {
            break;
        }
        // std::clog << "Retx Timer:" << current_timer.count() << "\n";
        // std::clog << "Retx Delay:" << rtx_delay.count() << "\n";

        current_timer = reTransmitWork(current_timer,
                                       std::chrono::steady_clock::now());
        if (current_timer.count() < 10)
        {
            // TODO: fix 60 to be an right minimal value
            current_timer = std::chrono::milliseconds(10);
        }
    }
}

std::chrono::milliseconds RtxManager::reTransmitWork(
    std::chrono::milliseconds current_timer,
    std::chrono::time_point<std::chrono::steady_clock> now)
{
    // Go over the retx list; remove what is acked or stale
    size_t tx_index = 0;
    auto oldest_time = now;        // start from now - and work to olden times
    auto item = tx_list.begin();

    while (item != tx_list.end())
    {
        auto origseq = item->origTransportSeq;
        auto &origaux = aux_map[origseq];
        // if acked or packet ttl expired (relative to its first transmission,
        // erase the packet
        if (origaux.ack_status ||
            (std::chrono::duration_cast<std::chrono::milliseconds>(
                 now - origaux.send_time) >= packet_ttl))
        {
            for (const auto &rtx_seq : origaux.retx_seq_list)
            {
                aux_map.erase(rtx_seq);
            }
            origaux.retx_seq_list.clear();
            // erase the packet
            aux_map.erase(origseq);
            item = tx_list.erase(item);
        }
        else
        {
            // should this packet be transmitted...when was the most recent time
            // it was transmitted
            auto last_sent = origaux.send_time;
            if (item->validPointer)
            {
                last_sent =
                    aux_map[item->packet->transportSequenceNumber].send_time;
            }

            // retransmit if the packet has waited for atleast rtx_delay time
            auto item_in_queue_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_sent);
            if (item->validPointer && item_in_queue_duration > rtx_delay)
            {
                rtx_count++;
                item->validPointer = false;
                // std::clog << "Retx PT:" << item->packet->mediaType << "
                // Duraion:" << item_in_queue_duration.count() << " Seq:" <<
                // item->packet->transportSequenceNumber << "\n";
                if (transport)
                {
                    item->packet->retransmitted = true;
                    transport->send(move(item->packet));
                }
            }
            else
            {
                // is this the oldest packet in terms of sent time
                // need this to figure out nex cycle candidate
                if (last_sent < oldest_time)
                {
                    oldest_time = last_sent;
                }
            }
            ++item;
        }
    }
    // Send Metric of how many packets we transmitted in this interval
    // std::clog << "Retx Count:" << rtx_count << "\n" << "\n";
    recordMetric(MeasurementType::PacketRate_RTX, NULL);
    rtx_count = 0;

    // Went through the transmit queue...set the timer and wait
    auto val = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - oldest_time);
    current_timer = rtx_delay -
                    (std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - oldest_time));
    return current_timer;
}

void RtxManager::recordMetric(MeasurementType mtype,
                              const PacketPointer &packetPointer)
{
    static const auto measurement_name = std::map<MeasurementType, std::string>{
        {MeasurementType::RTT_Smooth, "RttSmooth"},
        {MeasurementType::PacketRate_RTX, "RtxPacketCount"}};

    if (!metrics)
    {
        return;
    }

    if (!self_client_id)
    {
        std::clog << " RtxManager (recordMetric): ClientId isn't set, ignoring "
                     "...\n";
        return;
    }

    // common tags
    auto tags = Metrics::Measurement::Tags{
        {"clientID", self_client_id},
    };

    // fields common for most of the metrics
    auto fields = Metrics::Measurement::Fields{{"count", 1}};

    // These are created by default
    if (!measurements.count(MeasurementType::RTT_Smooth))
    {
        measurements[MeasurementType::RTT_Smooth] = metrics->createMeasurement(
            measurement_name.at(MeasurementType::RTT_Smooth), tags);
    }

    if (!measurements.count(MeasurementType::PacketRate_RTX))
    {
        measurements[MeasurementType::PacketRate_RTX] =
            metrics->createMeasurement(
                measurement_name.at(MeasurementType::PacketRate_RTX), tags);
    }

    auto now = std::chrono::system_clock::now();

    switch (mtype)
    {
        case MeasurementType::RTT_Smooth:
        {
            fields.push_back({"rtt_smooth", rtt.smooth});
            auto entry = Metrics::Measurement::TimeEntry{std::move(tags),
                                                         std::move(fields)};
            measurements[mtype]->set_time_entry(now, std::move(entry));
        }
        break;
        case MeasurementType::PacketRate_RTX:
        {
            fields.push_back({"rtx_num", rtx_count});
            auto entry = Metrics::Measurement::TimeEntry{std::move(tags),
                                                         std::move(fields)};
            measurements[mtype]->set_time_entry(now, std::move(entry));
        }
        break;
        default:
            break;
    }

    // TODO: throttle
    metrics->push();
}

}        // namespace neo_media

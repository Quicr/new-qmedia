#include "simple_video_assembler.hh"
#include <iostream>
#include <assert.h>

using namespace neo_media;

PacketPointer SimpleVideoAssembler::push(PacketPointer packet)
{
    // Get deque for timestamp.
    DepacketizerPointer deque;
    auto it = deques.find(packet->sourceRecordTime);
    if (it == deques.end())
    {
        deque = std::make_shared<Depacketizer>();
        deques.emplace(packet->sourceRecordTime, deque);
    }
    else
    {
        deque = it->second;
    }

    // Insert packet into dequeue.
    uint32_t packet_num = packet->chunkFragmentNum;
    uint32_t packet_count = packet->fragmentCount;
    auto pair = std::pair<uint32_t, PacketPointer>(packet_num,
                                                   std::move(packet));
    bool added = deque->Insert(std::move(pair));
    if (!added)
    {
        // TODO: Properly track rejected packet.
        std::cout << "Rejected packet" << std::endl;
    }

    // Are we done?
    if (!deque->Empty() && deque->Size() == packet_count &&
        deque->AllKeysConsecutive())
    {
        // Move parts into master packet.
        auto filled = std::make_unique<Packet>(*deque->Front().second.get());
        std::uint32_t estimatedSize = filled->frameSize * filled->fragmentCount;
        filled->data.clear();
        filled->data.reserve(estimatedSize);
        for (auto &item : deque->GetDeque())
        {
            // Move is no quicker on primitives but also no worse?
            auto &dest = filled->data;
            auto &source = item.second->data;
            dest.insert(dest.end(),
                        std::make_move_iterator(source.begin()),
                        std::make_move_iterator(source.end()));
            source.clear();
        }
        filled->frameSize = filled->data.size();

        // reset the packetize values.
        filled->fragmentCount = 1;
        filled->packetizeType = Packet::PacketizeType::None;
        filled->chunkFragmentNum = 0;

        deques.erase(filled->sourceRecordTime);
        decodeMedia(filled.get());
        return filled;
    }
    return nullptr;
}

void neo_media::SimpleVideoAssembler::decodeMedia(Packet *packet)
{
    // TODO: Here is your compressed video frame.
    return;
}

#pragma once

#include "qmedia/metrics.hh"
#include "qmedia/logger.hh"
#include "packet.hh"

namespace neo_media
{
class JitterInterface
{
public:
    using JitterIntPtr = std::shared_ptr<JitterInterface>;

    virtual ~JitterInterface(){};

    virtual bool push(PacketPointer packet) = 0;

    // TODO:  add protected video parameters for jitter subclasses
    virtual int popVideo(uint64_t sourceID,
                         uint32_t &width,
                         uint32_t &height,
                         uint32_t &format,
                         uint64_t &timestamp,
                         unsigned char **buffer,
                         Packet::IdrRequestData &idr_data_out) = 0;
    virtual PacketPointer popAudio(uint64_t sourceID, unsigned int length) = 0;
};

}        // namespace neo_media
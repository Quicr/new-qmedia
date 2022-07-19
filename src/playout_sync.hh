#pragma once

// Lip sync

#include "jitter_queues.hh"
#include <chrono>

namespace qmedia
{
class Sync
{
public:
    std::chrono::steady_clock::time_point local_audio_time_popped;
    uint64_t source_audio_time_popped = 0;
    uint64_t audio_seq_popped = 0;

    std::chrono::steady_clock::time_point local_video_time_popped;
    uint64_t source_video_time_popped = 0;
    uint64_t video_seq_popped = UINT64_MAX;

    enum sync_action
    {
        hold,
        pop,
        pop_discard,
        pop_video_only
    };
    void audio_popped(uint64_t source_time,
                      uint64_t seq,
                      std::chrono::steady_clock::time_point now);
    void video_popped(uint64_t source_time,
                      uint64_t seq,
                      std::chrono::steady_clock::time_point now);
    sync_action getVideoAction(unsigned int audio_pop_delay,
                               unsigned int video_pop_delay,
                               const MetaQueue &mq,
                               unsigned int &num_pop,
                               std::chrono::steady_clock::time_point now);
};
}        // namespace qmedia
#include <doctest/doctest.h>
#include <cstring>
#include <iostream>
#include "jitter.hh"
#include "jitter_queues.hh"
#include "opus_assembler.hh"

using namespace neo_media;

void pushPacket(Jitter &jitter,
                uint64_t seq,
                uint64_t sourceID,
                std::chrono::steady_clock::time_point now,
                bool silence)
{
    PacketPointer packet = std::make_unique<Packet>();
    packet->mediaType = Packet::MediaType::F32;
    packet->encodedSequenceNum = seq;
    packet->sourceID = sourceID;
    packet->clientID = 10;

    int audio_buff_size = 480 * 2 * sizeof(float);
    auto *buffer = new uint8_t[audio_buff_size];
    if (silence)
        memset(buffer, 0, audio_buff_size);
    else
        memset(buffer, 35, audio_buff_size);

    packet->data.resize(0);
    packet->data.reserve(audio_buff_size);
    std::copy(
        &buffer[0], &buffer[audio_buff_size], back_inserter(packet->data));
    jitter.push(std::move(packet), now);
}

void pushPacket(Jitter &jitter,
                uint64_t seq,
                uint64_t sourceID,
                std::chrono::steady_clock::time_point now)
{
    pushPacket(jitter, seq, sourceID, now, false);
}

void pushAndWork(Jitter &jitter,
                 uint64_t seq,
                 uint64_t sourceID,
                 std::chrono::steady_clock::time_point &now)
{
    pushPacket(jitter, seq, sourceID, now);
    now += std::chrono::milliseconds(5);
    jitter.QueueMonitor(now);
    now += std::chrono::milliseconds(5);
    jitter.QueueMonitor(now);
}

void pushAndWorkVariableDelay(Jitter &jitter,
                              uint64_t seq,
                              uint64_t sourceID,
                              std::chrono::steady_clock::time_point &now)
{
    pushPacket(jitter, seq, sourceID, now);
    now += std::chrono::milliseconds(5);
    jitter.QueueMonitor(now);
    now += std::chrono::milliseconds(5);
    jitter.QueueMonitor(now);
}

TEST_CASE("ordered_queue_inserts")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);

    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    // indicate client is active by popping
    PacketPointer packet = jitter.popAudio(sourceID, 3840, clock);

    for (int i = 0; i < 10; i++)
    {
        pushPacket(jitter, i, sourceID, clock);
        clock += std::chrono::milliseconds(10);
    }

    for (int i = 0; i < 10; i++)
    {
        packet = jitter.popAudio(sourceID, 3840, clock);
        CHECK_EQ(packet->encodedSequenceNum, i);
        packet.reset();
        clock += std::chrono::milliseconds(10);
    }
}

TEST_CASE("unordered_queue_inserts")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);

    int sourceID = 5;
    std::vector<uint64_t> seqs = {1, 3, 2, 7, 5, 6, 8, 10, 9, 4};
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    PacketPointer packet = jitter.popAudio(sourceID, 3840, clock);

    for (auto seq_num : seqs)
    {
        pushPacket(jitter, seq_num, sourceID, clock);
        clock += std::chrono::milliseconds(10);
    }

    int seq_num = 0;
    for (auto it : seqs)
    {
        ++seq_num;
        packet = jitter.popAudio(sourceID, 3840, clock);
        CHECK_EQ(packet->encodedSequenceNum, seq_num);
        packet.reset();
        clock += std::chrono::milliseconds(10);
    }
}

TEST_CASE("insert_opus_plc")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    PacketPointer packet = jitter.popAudio(sourceID, 3840, clock);

    std::vector<int> seq = {1, 3, 6, 11};
    for (auto seq_num : seq)
    {
        pushPacket(jitter, seq_num, sourceID, clock);
        clock += std::chrono::milliseconds(10);
    }

    int seq_num = 0;
    for (int i = 0; i < 11; i++)
    {
        ++seq_num;

        std::shared_ptr<MetaFrame> frame = jitter.audio.mq.front();
        if (std::find(std::begin(seq), std::end(seq), seq_num) != std::end(seq))
            CHECK_EQ(frame->type, MetaFrame::Type::media);
        else
            CHECK_EQ(frame->type, MetaFrame::Type::plc_gen);

        packet = jitter.popAudio(sourceID, 3840, clock);
        CHECK_EQ(packet->encodedSequenceNum, seq_num);
        packet.reset();
        clock += std::chrono::milliseconds(10);
    }
}

TEST_CASE("dont_jitter_old_packet")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    pushPacket(jitter, 1, sourceID, clock);
    clock += std::chrono::milliseconds(10);

    pushPacket(jitter, 2, sourceID, clock);
    clock += std::chrono::milliseconds(10);

    PacketPointer packet = jitter.popAudio(sourceID, 3840, clock);
    CHECK_EQ(1, packet->encodedSequenceNum);

    pushPacket(jitter, 1, sourceID, clock);
    clock += std::chrono::milliseconds(10);

    packet = jitter.popAudio(sourceID, 3840, clock);
    CHECK_EQ(2, packet->encodedSequenceNum);
}

TEST_CASE("inital_hold_back")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    pushAndWork(jitter, 1, sourceID, clock);
    PacketPointer packet = jitter.popAudio(sourceID, 3840, clock);
    CHECK_EQ(packet->encodedSequenceNum, 0);

    pushAndWork(jitter, 2, sourceID, clock);
    packet = jitter.popAudio(sourceID, 3840, clock);
    CHECK_EQ(packet->encodedSequenceNum, 1);

    pushAndWork(jitter, 3, sourceID, clock);
    packet = jitter.popAudio(sourceID, 3840, clock);
    CHECK_EQ(packet->encodedSequenceNum, 2);
}

TEST_CASE("adjust_jitter_no_pop")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    for (int i = 0; i < 10; i++)
    {
        pushAndWork(jitter, i, sourceID, clock);
    }
    CHECK_EQ(jitter.audio.mq.size(), 2);

    // jitter.setDelayMode(Jitter::DelayMode::listen);
    for (int i = 10; i < 30; i++)
    {
        pushAndWork(jitter, i, sourceID, clock);
    }
    CHECK_EQ(jitter.audio.mq.size(), 15);
}

TEST_CASE("update_jitter_values")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    uint64_t sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    for (int i = 1; i < 20; i++)
    {
        pushPacket(jitter, i, sourceID, clock);
        clock += std::chrono::milliseconds(10 + i);
    }
    jitter.audio_jitter.updateJitterValues(jitter.audio.mq,
                                           jitter.audio.getMsPerAudioPacket());

    auto pop_items = jitter.audio_jitter.jitter_values.size();
    for (size_t i = 0; i < pop_items; i++)
    {
        long value = jitter.audio_jitter.jitter_values.front();
        CHECK_EQ(i + 1, value);
        jitter.audio_jitter.jitter_values.pop_front();
    }
}

TEST_CASE("correlation_coefficient_flat")
{
    LeakyBucket bucket;
    LeakyBucket::Tracker tracker;

    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    for (int i = 0; i < 100; i++)
    {
        tracker.emplace_back(1, clock);
        clock += std::chrono::milliseconds(10);
    }
}

TEST_CASE("correlation_coefficient_increasing")
{
    LeakyBucket bucket;
    LeakyBucket::Tracker tracker;

    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    for (int i = 0; i < 100; i++)
    {
        tracker.emplace_back(i, clock);
        clock += std::chrono::milliseconds(10);
    }
}

TEST_CASE("correlation_coefficient_decrasing")
{
    LeakyBucket bucket;
    LeakyBucket::Tracker tracker;

    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    for (int i = 100; i > 0; i--)
    {
        tracker.emplace_back(i, clock);
        clock += std::chrono::milliseconds(10);
    }
}

TEST_CASE("correlation_prefect_flow_active_speaker")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    for (int i = 1; i < 100; i++)
    {
        pushAndWork(jitter, i, sourceID, clock);
        auto packet = jitter.popAudio(sourceID, 3840, clock);
        packet.reset();
    }
}

TEST_CASE("correlation_prefect_flow_listener")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    // jitter.setDelayMode(Jitter::DelayMode::listen);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    pushAndWork(jitter, 1, sourceID, clock);
    pushAndWork(jitter, 2, sourceID, clock);

    for (int i = 3; i < 300; i++)
    {
        pushAndWork(jitter, i, sourceID, clock);
        auto packet = jitter.popAudio(sourceID, 3840, clock);
        packet.reset();
    }
}

TEST_CASE("sensible_src_ratio_for_pefect_stream")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    // jitter.setDelayMode(Jitter::DelayMode::listen);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    pushAndWork(jitter, 1, sourceID, clock);
    pushAndWork(jitter, 2, sourceID, clock);

    for (int i = 3; i < 30; i++)
    {
        pushAndWork(jitter, i, sourceID, clock);
        auto packet = jitter.popAudio(sourceID, 3840, clock);
        packet.reset();
    }
    pushAndWork(jitter, 30, sourceID, clock);
    auto packet = jitter.popAudio(sourceID, 3840, clock);
    packet.reset();

    clock += std::chrono::milliseconds(5);
    jitter.QueueMonitor(clock);

    double src_ratio = jitter.bucket.getSrcRatio();
    REQUIRE(src_ratio == doctest::Approx(1.0));
}

TEST_CASE("normal_src_ratio_small_burst")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    pushAndWork(jitter, 1, sourceID, clock);
    pushAndWork(jitter, 2, sourceID, clock);
    auto packet = jitter.popAudio(sourceID, 3840, clock);
    packet.reset();
    clock += std::chrono::milliseconds(5);
    jitter.QueueMonitor(clock);

    for (int i = 3; i < 6; i++)
    {
        pushPacket(jitter, i, sourceID, clock);
    }
    clock += std::chrono::milliseconds(5);
    jitter.QueueMonitor(clock);
    clock += std::chrono::milliseconds(5);
    jitter.QueueMonitor(clock);

    double src_ratio = jitter.bucket.getSrcRatio();
    REQUIRE(src_ratio == doctest::Approx(1.0));
}

TEST_CASE("normal_src_ratio_larger_burst_but_not_allowed_to_change_yet")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    pushAndWork(jitter, 1, sourceID, clock);
    pushAndWork(jitter, 2, sourceID, clock);
    auto packet = jitter.popAudio(sourceID, 3840, clock);
    packet.reset();
    clock += std::chrono::milliseconds(5);
    jitter.QueueMonitor(clock);

    for (int i = 3; i < 8; i++)
    {
        pushPacket(jitter, i, sourceID, clock);
    }
    clock += std::chrono::milliseconds(5);
    jitter.QueueMonitor(clock);
    clock += std::chrono::milliseconds(5);
    jitter.QueueMonitor(clock);

    double src_ratio = jitter.bucket.getSrcRatio();
    REQUIRE(src_ratio == doctest::Approx(1.0));
}

TEST_CASE("burst_80_ms_increase_buffer_size")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    pushAndWork(jitter, 1, sourceID, clock);
    pushAndWork(jitter, 2, sourceID, clock);

    for (int i = 3; i < 100; i++)
    {
        pushAndWork(jitter, i, sourceID, clock);
        auto packet = jitter.popAudio(sourceID, 3840, clock);
        packet.reset();
    }
    clock += std::chrono::milliseconds(7 * 10);
    for (int i = 100; i < 108; i++)
    {
        pushPacket(jitter, i, sourceID, clock);
    }
    clock += std::chrono::milliseconds(5);
    jitter.QueueMonitor(clock);
    clock += std::chrono::milliseconds(5);
    auto packet = jitter.popAudio(sourceID, 3840, clock);
    packet.reset();
    jitter.QueueMonitor(clock);

    unsigned int queue_size = jitter.audio.mq.size();
    CHECK_EQ(queue_size, 8);
}

TEST_CASE("empty_pop_test_splash_80ms_resample_20ms")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    pushAndWork(jitter, 1, sourceID, clock);
    pushAndWork(jitter, 2, sourceID, clock);

    // normal flow
    for (int i = 3; i < 100; i++)
    {
        pushAndWork(jitter, i, sourceID, clock);
        auto packet = jitter.popAudio(sourceID, 3840, clock);
        packet.reset();
    }

    // huge wifi jitter - no packets coming in
    for (int i = 100; i < 108; i++)
    {
        clock += std::chrono::milliseconds(10);
        auto packet = jitter.popAudio(sourceID, 3840, clock);        // empty
                                                                     // pops -
                                                                     // not good
                                                                     // stuff
        packet.reset();
    }

    // burst of packets coming in
    for (int i = 100; i < 108; i++)
    {
        pushPacket(jitter, i, sourceID, clock);
    }

    CHECK_EQ(jitter.audio.mq.size(), 8);

    // normal flow
    for (int i = 108; i < 500; i++)
    {
        pushAndWork(jitter, i, sourceID, clock);
        auto packet = jitter.popAudio(sourceID, 3840, clock);
        packet.reset();

        // simulate 10% incrased jitter speed by poping extra every 10th packet
        if (jitter.bucket.getSrcRatio() < 1.0)
        {
            if ((i % 10) == 0)
            {
                auto packet = jitter.popAudio(sourceID, 3840, clock);
                packet.reset();
            }
        }
    }
    pushAndWork(jitter, 300, sourceID, clock);
    auto packet = jitter.popAudio(sourceID, 3840, clock);
    packet.reset();

    CHECK_EQ(jitter.audio.mq.size(), 0);
}

TEST_CASE("audio_frame_sizes")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    CHECK_EQ(jitter.audio.getFrameSize(), 3840);

    Jitter jitter2(
        48000, 1, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    CHECK_EQ(jitter2.audio.getFrameSize(), 1920);

    Jitter jitter3(
        41000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    CHECK_EQ(jitter3.audio.getFrameSize(), 3280);
}

TEST_CASE("remove_200ms_silence_from_1000ms")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    // indicate client is active by popping
    PacketPointer packet = jitter.popAudio(sourceID, 3840, clock);

    for (int i = 0; i < 21; i++)
    {
        pushPacket(jitter, i, sourceID, clock, true);
        clock += std::chrono::milliseconds(10);
    }

    int queue_size = 21;

    for (int i = 0; i < 100; i++)
    {
        packet = jitter.popAudio(sourceID, 3840, clock);
        CHECK_LE(jitter.audio.mq.size(), queue_size);
        queue_size = jitter.audio.mq.size();
        packet.reset();
        pushPacket(jitter, i + 21, sourceID, clock, true);
        clock += std::chrono::milliseconds(10);
    }
    CHECK_EQ(jitter.audio.mq.size(), 2);
}

TEST_CASE("inject_silence_after_listen_mode")
{
    Jitter jitter(
        48000, 2, Packet::MediaType::F32, 1920, 1080, 1, nullptr, nullptr);
    int sourceID = 5;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();

    // indicate client is active by popping
    PacketPointer packet = jitter.popAudio(sourceID, 3840, clock);

    int queue_size = jitter.audio.mq.size();

    for (int i = 0; i < 21; i++)
    {
        CHECK_GE(jitter.audio.mq.size(), queue_size);
        packet = jitter.popAudio(sourceID, 3840, clock);
        pushPacket(jitter, i, sourceID, clock, true);
        clock += std::chrono::milliseconds(10);
        queue_size = jitter.audio.mq.size();
    }

    // jitter.setDelayMode(JitterInterface::DelayMode::listen);
    for (int i = 21; i < 42; i++)
    {
        CHECK_GE(jitter.audio.mq.size(), queue_size);
        packet = jitter.popAudio(sourceID, 3840, clock);
        pushPacket(jitter, i, sourceID, clock, true);
        clock += std::chrono::milliseconds(10);
        queue_size = jitter.audio.mq.size();
    }
}
#include <doctest/doctest.h>
#include "playout_sync.hh"
#include "jitter_queues.hh"

using namespace neo_media;

void pushVideoPacket(MetaQueue &queue,
                     uint64_t seq,
                     uint64_t sourceID,
                     Packet::VideoFrameType frame_type,
                     uint64_t source_time,
                     std::chrono::steady_clock::time_point now)
{
    auto frame = std::make_shared<MetaFrame>(MetaFrame::Type::media, now);
    frame->packet = std::make_unique<Packet>();
    frame->packet->mediaType = Packet::MediaType::AV1;
    frame->packet->encodedSequenceNum = seq;
    frame->packet->sourceID = sourceID;
    frame->packet->clientID = 10;
    frame->packet->videoFrameType = frame_type;
    frame->packet->sourceRecordTime = source_time;

    int video_frame_size = 1000;
    auto *buffer = new uint8_t[video_frame_size];
    memset(buffer, 0, video_frame_size);
    frame->packet->data.resize(0);
    frame->packet->data.reserve(video_frame_size);
    std::copy(&buffer[0],
              &buffer[video_frame_size],
              back_inserter(frame->packet->data));
    queue.Q.push_back(frame);
}

TEST_CASE("sync_initial_pop_IDR_video_only")
{
    Sync sync;
    MetaQueue mq;
    unsigned int num_pop;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();
    uint64_t source_time =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();

    pushVideoPacket(mq, 1, 5, Packet::VideoFrameType::Idr, source_time, clock);
    Sync::sync_action action = sync.getVideoAction(10, 33, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::pop);
}

TEST_CASE("sync_discard_until_IDR_video_only")
{
    Sync sync;
    MetaQueue mq;
    unsigned int num_pop;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();
    uint64_t source_time =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();

    for (int i = 1; i < 5; i++)
    {
        pushVideoPacket(
            mq, i, 5, Packet::VideoFrameType::None, source_time, clock);
        clock += std::chrono::milliseconds(33);
    }
    pushVideoPacket(mq, 5, 5, Packet::VideoFrameType::Idr, source_time, clock);
    Sync::sync_action action = sync.getVideoAction(10, 33, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::pop_discard);
    CHECK_EQ(num_pop, 4);

    for (int i = 1; i < 5; i++)
    {
        mq.Q.pop_front();
    }
    num_pop = 0;
    action = sync.getVideoAction(10, 33, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::pop);
}

TEST_CASE("sync_pop_in_order_video_only")
{
    Sync sync;
    MetaQueue mq;
    unsigned int num_pop;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();
    Sync::sync_action action;

    uint64_t source_time =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();

    pushVideoPacket(mq, 1, 5, Packet::VideoFrameType::Idr, source_time, clock);
    action = sync.getVideoAction(10, 33, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::pop);
    CHECK_EQ(num_pop, 1);
    sync.video_popped(source_time, mq.getNextSeq(), clock);
    mq.Q.pop_front();

    for (int i = 2; i < 10; i++)
    {
        clock += std::chrono::milliseconds(33);
        source_time += 33000;
        pushVideoPacket(
            mq, i, 5, Packet::VideoFrameType::None, source_time, clock);
        action = sync.getVideoAction(10, 33, mq, num_pop, clock);
        CHECK_EQ(action, Sync::sync_action::pop_video_only);
        CHECK_EQ(num_pop, 1);
        sync.video_popped(source_time, mq.getNextSeq(), clock);
        mq.Q.pop_front();
    }
}

TEST_CASE("sync_freeze_until_IDR_video_only")
{
    Sync sync;
    MetaQueue mq;
    unsigned int num_pop;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();
    Sync::sync_action action;

    uint64_t source_time =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    sync.video_popped(source_time, 1, clock);        // avoid initial pop - and
                                                     // initial IDR

    unsigned int to_pop = 0;
    for (int seq = 3; seq < 10; seq++)
    {
        pushVideoPacket(
            mq, seq, 5, Packet::VideoFrameType::None, source_time, clock);
        action = sync.getVideoAction(10, 33, mq, num_pop, clock);
        CHECK_EQ(action, Sync::sync_action::pop_discard);
        CHECK_EQ(num_pop, ++to_pop);
    }
    pushVideoPacket(mq, 10, 5, Packet::VideoFrameType::Idr, source_time, clock);
    action = sync.getVideoAction(10, 33, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::pop_discard);
    CHECK_EQ(num_pop, to_pop);

    for (unsigned int pops = 0; pops < to_pop; pops++)
    {
        sync.video_popped(mq.getNextSourceTime(), mq.getNextSeq(), clock);
        mq.Q.pop_front();
    }
    action = sync.getVideoAction(10, 33, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::pop_video_only);
    CHECK_EQ(num_pop, 1);
}

TEST_CASE("sync_follow_audio_exact_match")
{
    Sync sync;
    MetaQueue mq;
    unsigned int num_pop;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();
    Sync::sync_action action;

    uint64_t source_time =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    sync.video_popped(source_time, 1, clock);
    pushVideoPacket(
        mq, 2, 5, Packet::VideoFrameType::Idr, source_time + 33000, clock);
    pushVideoPacket(
        mq, 3, 5, Packet::VideoFrameType::Idr, source_time + 66000, clock);

    int seq = 1;
    sync.audio_popped(source_time, seq++, clock);
    action = sync.getVideoAction(10000, 33000, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::hold);
    CHECK_EQ(num_pop, 0);

    source_time += 10000;
    sync.audio_popped(source_time, seq++, clock);        // 10000 microseconds
    action = sync.getVideoAction(10000, 33000, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::hold);
    CHECK_EQ(num_pop, 0);

    source_time += 10000;
    sync.audio_popped(source_time, seq++, clock);        // 10000 microseconds
    action = sync.getVideoAction(10000, 33000, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::hold);
    CHECK_EQ(num_pop, 0);

    source_time += 10000;
    sync.audio_popped(source_time, seq++, clock);        // 10000 microseconds
    action = sync.getVideoAction(10000, 33000, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::hold);
    CHECK_EQ(num_pop, 0);

    source_time += 10000;
    sync.audio_popped(source_time, seq++, clock);        // 10000 microseconds
    action = sync.getVideoAction(10000, 33000, mq, num_pop, clock);
    CHECK_EQ(action, Sync::sync_action::pop);
    CHECK_EQ(num_pop, 1);
    sync.video_popped(mq.getNextSourceTime(), mq.getNextSeq(), clock);
    mq.Q.pop_front();
}

TEST_CASE("sync_follow_audio_10ms_30fps")
{
    Sync sync;
    MetaQueue mq;
    unsigned int num_pop;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();
    Sync::sync_action action;

    uint64_t source_start_time =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();

    int audio_seq = 1;
    int video_seq = 1;
    uint64_t audio_interval = 10000;
    uint64_t video_interval = 33333;
    sync.video_popped(source_start_time, video_seq++, clock);
    uint64_t video_time = source_start_time + video_interval;
    uint64_t three_seconds = source_start_time + 3000000;
    for (uint64_t time = source_start_time; time < three_seconds;
         time += audio_interval)
    {
        sync.audio_popped(time, audio_seq++, clock);

        if (video_time < time)
        {
            pushVideoPacket(mq,
                            video_seq,
                            5,
                            Packet::VideoFrameType::None,
                            video_time,
                            clock);
            action = sync.getVideoAction(
                audio_interval, video_interval, mq, num_pop, clock);
            CHECK_EQ(action, Sync::sync_action::pop);
            CHECK_EQ(num_pop, 1);
            sync.video_popped(video_time, video_seq, clock);
            ++video_seq;
            video_time += video_interval;
            mq.Q.pop_front();
        }
        else
        {
            action = sync.getVideoAction(
                audio_interval, video_interval, mq, num_pop, clock);
            CHECK_EQ(action, Sync::sync_action::hold);
            CHECK_FALSE(num_pop);
        }
    }
}

TEST_CASE("sync_follow_audio_IDR_freeze")
{
    Sync sync;
    MetaQueue mq;
    unsigned int num_pop;
    std::chrono::steady_clock::time_point clock =
        std::chrono::steady_clock::now();
    Sync::sync_action action;

    uint64_t source_start_time =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();

    int audio_seq = 1;
    int video_seq = 1;
    uint64_t audio_interval = 10000;
    uint64_t video_interval = 33333;
    uint64_t video_time = source_start_time + video_interval;
    uint64_t video_pop_time = video_time;
    uint64_t three_seconds = source_start_time + 3000000;
    sync.video_popped(source_start_time, video_seq++, clock);
    Packet::VideoFrameType frame = Packet::VideoFrameType::None;
    bool errorInStream = false;

    for (uint64_t time = (source_start_time + audio_interval);
         time < three_seconds;
         time += audio_interval)
    {
        sync.audio_popped(time, audio_seq++, clock);

        // every 20th frame is an IDR
        if (video_seq % 20 == 0)
        {
            frame = Packet::VideoFrameType::Idr;
            errorInStream = false;
        }
        else
            frame = Packet::VideoFrameType::None;

        // loose every 8th video frame
        if (video_seq % 8 == 0)
        {
            ++video_seq;
            video_time += video_interval;
            errorInStream = true;
        }

        if (video_pop_time < time)
        {
            pushVideoPacket(mq, video_seq, 5, frame, video_time, clock);

            action = sync.getVideoAction(
                audio_interval, video_interval, mq, num_pop, clock);
            if (errorInStream)
            {
                if (num_pop > 0)
                {
                    CHECK_EQ(action, Sync::sync_action::pop_discard);
                    for (unsigned int i = 0; i < num_pop; i++)
                    {
                        mq.Q.pop_front();
                    }
                }
            }
            else
            {
                CHECK_EQ(action, Sync::sync_action::pop);
                sync.video_popped(
                    mq.getNextSourceTime(), mq.getNextSeq(), clock);
                mq.Q.pop_front();
                ++video_seq;
                video_time += video_interval;
            }
            video_pop_time += video_interval;
        }
        else
        {
            if (errorInStream)
            {
                action = sync.getVideoAction(
                    audio_interval, video_interval, mq, num_pop, clock);
                if (num_pop > 0)
                {
                    CHECK_EQ(action, Sync::sync_action::pop_discard);
                    for (unsigned int i = 0; i < num_pop; i++)
                    {
                        mq.Q.pop_front();
                    }
                }
                else
                    CHECK_EQ(action, Sync::sync_action::hold);
            }
            else
            {
                action = sync.getVideoAction(
                    audio_interval, video_interval, mq, num_pop, clock);
                CHECK_EQ(action, Sync::sync_action::hold);
            }
        }
    }
}

TEST_CASE("video only switch after audio stopping")
{
    Sync sync;
    MetaQueue mq;
    unsigned int num_pop;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    Sync::sync_action action;

    uint64_t source_start_time =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    source_start_time -= 200 * 1000;        // source time happens before pop
                                            // time

    uint64_t audio_seq = 1;
    uint64_t video_seq = 1;

    sync.audio_popped(source_start_time, audio_seq++, now);
    source_start_time += 23 * 1000;
    now += std::chrono::milliseconds(13);
    pushVideoPacket(mq,
                    video_seq,
                    1,
                    Packet::VideoFrameType::Idr,
                    source_start_time,
                    now);        // initial packet to be popped
    mq.pop(now);
    sync.video_popped(source_start_time, video_seq, now);        // popping IDR
    pushVideoPacket(mq,
                    ++video_seq,
                    1,
                    Packet::VideoFrameType::None,
                    source_start_time,
                    now);        // no new audio - this packet will be popped
                                 // after 400ms

    // no more popping audio - let's start popping video
    for (uint64_t new_source = source_start_time;
         new_source < (source_start_time + (33000 * 11));
         new_source += 33000)
    {
        now += std::chrono::milliseconds(33);
        action = sync.getVideoAction(0, 0, mq, num_pop, now);
        CHECK_EQ(action, Sync::sync_action::hold);
        ++video_seq;
    }

    now += std::chrono::milliseconds(33);
    action = sync.getVideoAction(0, 0, mq, num_pop, now);
    CHECK_EQ(action, Sync::sync_action::pop_video_only);
}
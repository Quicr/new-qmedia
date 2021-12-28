#include <doctest/doctest.h>
#include "audio_encoder.hh"

using namespace neo_media;

void callback_dummy(PacketPointer packet)
{
}

TEST_CASE("fullFillMatch")
{
    fullFill buffers;
    const int length = 480 * 1 * sizeof(uint16_t);
    uint8_t input_buffer[length];
    std::vector<uint8_t> fill_buff;
    uint64_t time = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
    uint64_t time_inc = 10000;        // 10ms

    uint64_t fill_time;

    fill_buff.resize(0);
    buffers.addBuffer(input_buffer, length, time);
    CHECK(buffers.fill(fill_buff, length, fill_time));
    fill_buff.resize(0);
    CHECK_FALSE(buffers.fill(fill_buff, length, fill_time));        // too early
                                                                    // to pull
                                                                    // again
    fill_buff.resize(0);
    buffers.addBuffer(input_buffer, length, time + time_inc);
    CHECK(buffers.fill(fill_buff, length, fill_time));
    fill_buff.resize(0);
}

TEST_CASE("fullFillAppleSizeSend")
{
    fullFill buffers;

    const int input_length = 512 * 1 * sizeof(uint16_t);
    uint8_t input_buffer[input_length];

    const int fill_length = 480 * 1 * sizeof(uint16_t);
    std::vector<uint8_t> fill_buffer;

    uint64_t time = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
    uint64_t add_time = 10667;

    uint64_t fill_time;

    buffers.addBuffer(input_buffer, input_length, time);
    CHECK(buffers.fill(fill_buffer, fill_length, fill_time));
    fill_buffer.resize(0);
    buffers.addBuffer(input_buffer, input_length, time + (1 * add_time));
    CHECK(buffers.fill(fill_buffer, fill_length, fill_time));
    fill_buffer.resize(0);
    buffers.addBuffer(input_buffer, input_length, time + (2 * add_time));
    CHECK(buffers.fill(fill_buffer, fill_length, fill_time));
    fill_buffer.resize(0);
    buffers.addBuffer(input_buffer, input_length, time + (3 * add_time));
    CHECK(buffers.fill(fill_buffer, fill_length, fill_time));
    fill_buffer.resize(0);
}

TEST_CASE("fullFillAppleRace")
{
    fullFill buffers;
    const int input_length = 512 * 1 * sizeof(uint16_t);
    uint8_t input_buffer[input_length];
    const int fill_length = 480 * 1 * sizeof(uint16_t);
    std::vector<uint8_t> fill_buffer;

    uint64_t time = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
    uint64_t add_time = 10667;
    uint64_t fill_time;

    buffers.addBuffer(input_buffer, input_length, time);

    const unsigned int read_samples = 480 * 1;
    const unsigned int write_samples = 512 * 1;
    bool success = true;

    for (uint64_t samples = 0; samples < 48000; samples++)
    {
        if ((samples % read_samples) == 0)
        {
            time += add_time;
            fill_buffer.resize(0);
            bool ret = buffers.fill(fill_buffer, fill_length, fill_time);
            if (success) success = ret;
        }

        if ((samples % write_samples) == 0)
        {
            time += add_time;
            buffers.addBuffer(input_buffer, input_length, time);
        }
    }
    CHECK_EQ(success, true);
}

TEST_CASE("fullFillAppleRace2Channels")
{
    fullFill buffers;
    const int input_length = 512 * 2 * sizeof(uint16_t);
    uint8_t input_buffer[input_length];
    const int fill_length = 480 * 2 * sizeof(uint16_t);
    std::vector<uint8_t> fill_buffer;

    uint64_t time = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
    uint64_t add_time = 10667;
    uint64_t fill_time;

    buffers.addBuffer(input_buffer, input_length, time);

    const unsigned int read_samples = 480 * 2;
    const unsigned int write_samples = 512 * 2;
    bool success = true;

    for (uint64_t samples = 0; samples < 48000; samples++)
    {
        if ((samples % read_samples) == 0)
        {
            fill_buffer.resize(0);
            bool ret = buffers.fill(fill_buffer, fill_length, fill_time);
            if (success) success = ret;
        }

        if ((samples % write_samples) == 0)
        {
            time += add_time;
            buffers.addBuffer(input_buffer, input_length, time);
        }
    }
    CHECK_EQ(success, true);
}

TEST_CASE("fullFillSamplesToMicroseconds")
{
    fullFill buffers;
    int channels = 2;
    buffers.sample_divisor = channels * sizeof(float);

    uint64_t time = 1000000;
    unsigned int read_front = 0;
    CHECK_EQ(time, buffers.calculate_timestamp(read_front, time));

    read_front = channels * sizeof(float);        // single sample
    CHECK_EQ(time + 20, buffers.calculate_timestamp(read_front, time));

    read_front = 10 * channels * sizeof(float);
    CHECK_EQ(time + 208, buffers.calculate_timestamp(read_front, time));
}
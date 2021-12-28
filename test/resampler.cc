#include <doctest/doctest.h>
#include <fstream>
#include <iostream>
#include "resampler.hh"

using namespace neo_media;

void runResampleOnFile(const int frame_size, double ratio)
{
    std::ifstream sound_file;

    sound_file.open(DATA_PATH + std::string("test_sound-float32-1-2633.csv"));
    CHECK_FALSE(sound_file.fail());

    std::vector<float> values;
    float value;
    char delim;
    if (sound_file.is_open())
    {
        while (!sound_file.eof())
        {
            sound_file >> value;
            values.push_back(value);
            sound_file >> delim;
        }

        Resampler resampler;
        for (unsigned int i = 0; i < values.size(); i += frame_size)
        {
            PacketPointer packet = std::make_unique<Packet>();
            packet->mediaType = Packet::MediaType::F32;
            packet->encodedSequenceNum = 1;
            packet->data.resize(0);
            int size_bytes = frame_size * sizeof(float);
            packet->data.reserve(size_bytes);
            uint8_t *bytes = (uint8_t *) &values[i];
            std::copy(bytes, bytes + size_bytes, back_inserter(packet->data));

            int pre_size = packet->data.size();
            resampler.resample(packet.get(), 1, ratio);
            int post_size = packet->data.size();
            CHECK_EQ(ratio * pre_size, post_size);
            packet.reset();
        }
        sound_file.close();
    }
}

TEST_CASE("upsample_shorten_2ms_on_10ms_frame")
{
    runResampleOnFile(480, 0.8);
}

TEST_CASE("upsample_shorten_10ms_on_20ms_frame")
{
    runResampleOnFile(960, 0.5);
}

TEST_CASE("downsample_extend_2ms_on_10ms_frame")
{
    runResampleOnFile(480, 1.2);
}
TEST_CASE("downsample_extend_10ms_on_20ms_frame")
{
    runResampleOnFile(960, 1.5);
}

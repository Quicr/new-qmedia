#include <doctest/doctest.h>
#include <iostream>

#include "qmedia/neo_media_client.hh"

void sub_cb(std::uint64_t id, std::uint8_t media_id, std::uint16_t client_id, std::uint8_t * /*data*/, std::uint32_t length, uint64_t /*timestamp*/)
{
    std::cerr << "callback id " << id << 
        "\n\tlength " << length << 
        "\n\tclient id " << std::hex << client_id <<
        "\n\tmedia id " << (int)media_id << std::endl;
}

TEST_CASE("neo create and destroy (UDP)")
{
    void *media_client;
    MediaClient_Create("127.0.0.1", 1234, 0, &media_client);
    CHECK_NE(media_client, nullptr);

    MediaClient_Destroy(media_client);
}

TEST_CASE("neo subscribe  (UDP)")
{
    void *media_client;
    MediaClient_Create("127.0.0.1", 1234, 0, &media_client);
    CHECK_NE(media_client, nullptr);    

    uint64_t audioStreamSubId = MediaClient_AddAudioStreamSubscribe(media_client, 0x80, sub_cb);
    CHECK_NE(audioStreamSubId, 0);

    MediaClient_RemoveMediaSubscribeStream(media_client, audioStreamSubId);

    MediaClient_Destroy(media_client);
}


#include <cstdint>
#include <iostream>
#include <unistd.h>

#include <qmedia/neo_media_client.hh>

void sub_cb(std::uint64_t id,
            std::uint8_t media_id,
            std::uint16_t client_id,
            std::uint8_t* /*data*/,
            std::uint32_t length,
            uint64_t /*timestamp*/)
{
    std::cerr << "callback id " << id << "\n\tlength " << length << "\n\tclient id " << std::hex << client_id
              << "\n\tmedia id " << (int) media_id << std::endl;
}

int main(int /*argc*/, char** /*argv*/)
{
    uint8_t protocol = 1;

    void* sub_handle = 0;
    MediaClient_Create("127.0.0.1", 1234, protocol /*QUIC*/, &sub_handle);

    std::uint64_t sub_audio_streamId = MediaClient_AddStreamSubscribe(sub_handle, 1, 1, 8, sub_cb);
    std::uint64_t sub_video_streamId = MediaClient_AddStreamSubscribe(sub_handle, 1, 2, 9, sub_cb);

    std::cerr << "sub audio - id " << sub_audio_streamId << std::endl;
    std::cerr << "sub video - id " << sub_video_streamId << std::endl;

    void* pub_handle = 0;
    MediaClient_Create("127.0.0.1", 1234, protocol /*QUIC*/, &pub_handle);

    std::uint64_t pub_audio_streamId = MediaClient_AddStreamPublishIntent(pub_handle, 1, 0x01, 0xABCD);
    std::uint64_t pub_video_streamId = MediaClient_AddStreamPublishIntent(pub_handle, 1, 0x02, 0xABCD);

    std::cerr << "pub audio - id " << pub_audio_streamId << std::endl;
    std::cerr << "pub video - id " << pub_video_streamId << std::endl;

    std::uint64_t timestamp = 0;
    std::uint16_t length = 256;
    char abuffer[256];
    char vbuffer[256];

    int iterations = 0;

    while (iterations <= 5)
    {
        MediaClient_sendAudio(pub_handle, pub_audio_streamId, abuffer, length, timestamp);

        MediaClient_sendVideoFrame(pub_handle, pub_video_streamId, vbuffer, length, timestamp, true);
        sleep(1);
        ++iterations;
    }

    MediaClient_RemoveMediaSubscribeStream(sub_handle, sub_audio_streamId);

    MediaClient_sendAudio(pub_handle, pub_audio_streamId, abuffer, length, timestamp);

    MediaClient_sendVideoFrame(pub_handle, pub_video_streamId, vbuffer, length, timestamp, true);

    MediaClient_sendAudio(pub_handle, pub_audio_streamId, abuffer, length, timestamp);

    MediaClient_sendVideoFrame(pub_handle, pub_video_streamId, vbuffer, length, timestamp, true);

    MediaClient_RemoveMediaPublishStream(pub_handle, pub_video_streamId);

    MediaClient_Destroy(sub_handle);
    MediaClient_Destroy(pub_handle);
}

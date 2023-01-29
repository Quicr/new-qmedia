
#include <cstdint>
#include <iostream>
#include <unistd.h>

#include <qmedia/neo_media_client.hh>

void sub_cb(std::uint64_t id, std::uint8_t *data, std::uint32_t length)
{
    std::cerr << "callback" << std::endl;
}

int main(int argc, char *argv[])
{
    void *sub_handle = 0;
    MediaClient_Create("127.0.0.1", 1234, &sub_handle);

    std::uint64_t sub_audio_streamId = MediaClient_AddAudioStreamSubscribe(sub_handle, 0, sub_cb);
    std::uint64_t sub_video_streamId = MediaClient_AddVideoStreamSubscribe(sub_handle, 0, sub_cb);

    std::cerr << "sub audio - id " << sub_audio_streamId << std::endl;
    std::cerr << "sub video - id " << sub_video_streamId << std::endl;

    void *pub_handle = 0;
    MediaClient_Create("127.0.0.1", 1234, &pub_handle);

    std::uint64_t pub_audio_streamId = MediaClient_AddAudioStreamPublishIntent(pub_handle, 0);
    std::uint64_t pub_video_streamId = MediaClient_AddVideoStreamPublishIntent(pub_handle, 0);

    std::cerr << "pub audio - id " << pub_audio_streamId << std::endl;
    std::cerr << "pub video - id " << pub_video_streamId << std::endl;

    std::uint64_t timestamp = 0;
    std::uint16_t length = 256;
    char buffer[256];
    
    MediaClient_sendAudio(pub_handle,
                          pub_audio_streamId,
                          buffer,
                          length,
                          timestamp);

    MediaClient_sendVideoFrame(pub_handle,
                          pub_video_streamId,
                          buffer,
                          length,
                          timestamp, true);


    while(true) {
        MediaClient_sendAudio(pub_handle,
                          pub_audio_streamId,
                          buffer,
                          length,
                          timestamp);

        MediaClient_sendVideoFrame(pub_handle,
                          pub_video_streamId,
                          buffer,
                          length,
                          timestamp, true);
        sleep(3);
    }
}

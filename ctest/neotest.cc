
#include "../src/extern/neo_media_client.hh"
#include <cstdint>
#include <iostream>


void sub_cb(std::uint64_t id, std::uint8_t *data, std::uint32_t length)
{
    std::cerr << "callback" << std::endl;
}

int main(int argc, char *argv[])
{
    void *sub_handle = 0;

    MediaClient_Create("127.0.0.1", 10080, &sub_handle);

    std::uint64_t sub_audio_streamId = MediaClient_AddAudioStreamSubscribe(sub_handle, 0, sub_cb);
    std::uint64_t sub_video_streamId = MediaClient_AddVideoStreamSubscribe(sub_handle, 0, sub_cb);

    void *pub_handle = 0;
    MediaClient_Create("127.0.0.1", 1234, &pub_handle);

    std::uint64_t pub_audio_streamId = MediaClient_AddAudioStreamPublishIntent(&pub_handle, 0);
    std::uint64_t pub_video_streamId = MediaClient_AddAudioStreamPublishIntent(&pub_handle, 0);

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


    while(true) {}
    
}

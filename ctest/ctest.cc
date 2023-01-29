
#include <iostream>

#include <qmedia/media_client.hh>

//typedef void(CALL *SubscribeCallback)(uint64_t id, uint8_t *data, uint32_t length);

void subscribe_callback(uint64_t id, uint8_t *data, uint32_t length)
{
    std::cerr << "subscribe_callback..." << id << std::endl;
}

int main(int argc, char *argv[])
{
    qmedia::MediaClient subClient("127.0.0.1", 1234, 0, nullptr);
    qmedia::MediaClient pubClient("127.0.0.1", 1234, 0, nullptr);

    subClient.add_audio_stream_subscribe(1, subscribe_callback);

    uint8_t buffer[256];
    pubClient.send_audio_media(1, 
                                buffer,
                                256,
                                0);

    while(true) {}

    return 0;
}

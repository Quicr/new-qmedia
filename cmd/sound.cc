
#include <iostream>
#include <mutex>
#include <portaudio.h>
#include <cstring>
#include <thread>
#include <random>
#include <ctime>
#include <fstream>
#include <sstream>
#include <chrono>

#include <qmedia/media_client.hh>
#include "../src/resampler.hh"
#include "qmedia/logger.hh"

using namespace qmedia;

static bool shutDown;

static PaStream* audioStream;
static std::mutex audioReadMutex;
static std::mutex audioWriteMutex;

static bool recvMedia = false;
static uint64_t recvClientID;
static uint64_t recvSourceID;

static const unsigned int remote_port = 7777;
static const unsigned int sample_rate = 48000;
static const unsigned int frames_per_buffer = 10 * 48;        // 10 ms
static const unsigned int audio_channels = 1;
static const unsigned int bytesPerSample = 4;
/**/
static const AudioConfig::SampleType sample_type = AudioConfig::SampleType::Float32;
* / static const double resample_ratio = 1.0;
static Resampler resampler;

static std::ofstream outg_sound_file;
static LoggerPointer logger = std::make_shared<Logger>("Sound", true);

void recordThreadFunc(MediaClient* client, MediaStreamId stream_id)
{
    int buff_size = frames_per_buffer * bytesPerSample * audio_channels;
    char* zerobuff = (char*) malloc(buff_size);
    if (zerobuff == nullptr)
    {
        logger->error << "Couldn't allocate zero buffer" << std::flush;
        return;
    }
    memset(zerobuff, 0, buff_size);
    uint64_t sourceID = 2;
    uint64_t timestamp = 0;

    while (!shutDown)
    {
        char* audioBuff = (char*) malloc(buff_size);
        if (audioBuff == nullptr)
        {
            logger->error << "Failed to allocate audio buffer" << std::flush;
            continue;
        }

        PaError err;
        {
            std::lock_guard<std::mutex> lock(audioReadMutex);
            while ((err = Pa_IsStreamActive(audioStream)) == 1)
            {
                long toRead = Pa_GetStreamReadAvailable(audioStream);
                printf("available: %ld frames_per_buffer: %d\n", toRead, frames_per_buffer);
                if (toRead == 0)
                {
                    Pa_Sleep(10);
                    continue;
                }
                if (toRead > frames_per_buffer) toRead = frames_per_buffer;

                if (toRead == frames_per_buffer)
                {
                    // You may get underruns or overruns if the output is not
                    // primed by PortAudio.
                    err = Pa_ReadStream(audioStream, audioBuff, toRead);
                    if (err)
                    {
                        logger->error << "Failed to read PA stream: " << Pa_GetErrorText(err) << std::flush;
                        continue;
                    }

                    if (memcmp(audioBuff, zerobuff, buff_size) == 0)
                    {
                        logger->debug << "0" << std::flush;
                    }

                    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();

                    client->send_audio(
                        stream_id, reinterpret_cast<uint8_t*>(const_cast<char*>(audioBuff)), buff_size, timestamp);
                    logger->debug << "-" << std::flush;
                    Pa_Sleep(10);
                }
            }
        }
        free(audioBuff);
    }
    free(zerobuff);
}

void playThreadFunc(MediaClient* client, MediaStreamId stream_id)
{
    std::chrono::steady_clock::time_point loop_time = std::chrono::steady_clock::now();

    while (!shutDown)
    {
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                           loop_time);
        logger->debug << "{" << delta.count() << "}" << std::flush;
        loop_time = std::chrono::steady_clock::now();

        if (!recvMedia)
        {
            logger->debug << "Z" << std::flush;
            std::chrono::steady_clock::time_point sleep_time = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            auto sleep_delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                     sleep_time);
            logger->debug << "{S: " << sleep_delta.count() << "}" << std::flush;
            continue;
        }

        long playoutSpaceAvailable;
        {
            std::lock_guard<std::mutex> lock(audioWriteMutex);
            playoutSpaceAvailable = Pa_GetStreamWriteAvailable(audioStream);
            logger->info << "[" << playoutSpaceAvailable << "]" << std::flush;
        }

        if (playoutSpaceAvailable < 1000 /* 21 ms */)
        {
            // not enought space to play, wait till later
            logger->info << "$" << std::flush;
            std::chrono::steady_clock::time_point sleep_time = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            auto sleep_delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                     sleep_time);
            logger->info << "{S: " << sleep_delta.count() << "}" << std::flush;
            continue;
        }

        int buff_size = frames_per_buffer * bytesPerSample * audio_channels;
        unsigned char* raw_data = nullptr;
        uint64_t timestamp;
        Packet* freePacket = nullptr;

        std::chrono::steady_clock::time_point get_audio = std::chrono::steady_clock::now();
        int recv_actual = client->get_audio(stream_id, timestamp, &raw_data, buff_size, nullptr);
        auto audio_delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                 get_audio);
        logger->info << "{A:" << audio_delta.count() << "}" << std::flush;

        PaError err;
        if (!recv_actual)
        {
            // write silence to the stream as not packet available
            assert(sizeof(float) == bytesPerSample);
            float plc[frames_per_buffer * audio_channels];
            std::fill(plc, plc + frames_per_buffer * audio_channels, 0.0);

            logger->debug << "*" << std::flush;
            {
                std::lock_guard<std::mutex> lock(audioWriteMutex);
                err = Pa_WriteStream(audioStream, plc, frames_per_buffer);
            }
            if (err)
            {
                logger->error << "Pa_WriteStream (plc) failed: " << Pa_GetErrorText(err) << std::flush;
                assert(0);        // TODO
            }
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        logger->debug << "+" << std::flush;
        {
            std::chrono::steady_clock::time_point start_write = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(audioWriteMutex);

            if (resample_ratio != 1.0)
            {
                float* src = (float*) raw_data;
                long input_frames = recv_actual / (bytesPerSample);
                long resampled_output_frames = input_frames * resample_ratio;
                float* resampled = new float[resampled_output_frames];
                int ret = resampler.simple_resample(
                    src, input_frames, resampled, resampled_output_frames, audio_channels, resample_ratio);

                if (ret)
                {
                    logger->error << "Resample failed: " << src_strerror(ret) << std::flush;
                }
                else
                {
                    err = Pa_WriteStream(
                        audioStream, (const void*) resampled, resampled_output_frames / audio_channels);
                }

                delete[] resampled;
            }
            else
            {
                err = Pa_WriteStream(audioStream, raw_data, recv_actual / (audio_channels * bytesPerSample));
                auto write_delta = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_write);
                logger->debug << "{W: " << write_delta.count() << "}" << std::flush;
            }
        }
        if (err)
        {
            logger->error << "Pa_WriteStream failed: " << Pa_GetErrorText(err) << std::flush;
        }

        // hack until Packet is split into meta and data
        std::chrono::steady_clock::time_point free_time = std::chrono::steady_clock::now();
        if (freePacket != nullptr)
        {
            delete (Packet*) freePacket;
        }
        auto free_delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                free_time);
        logger->debug << "{F: " << free_delta.count() << "}" << std::flush;
    }
}

int main(int argc, char* argv[])
{
    const uint64_t conference_id = 123456;
#if defined(_WIN32)
    timeBeginPeriod(1);        // timerstonk - push minimum resolution down to 1
                               // ms
#endif
    if (argc < 4)
    {
        std::cerr << "Must provide mode of operation" << std::endl;
        std::cerr << "Usage: sound <remote-address> <port> <mode> <name> "
                     "<source-id> "
                  << std::endl;
        std::cerr << "Mode: pub/sub/pubsub" << std::endl;
        std::cerr << "" << std::endl;
        return -1;
    }

    std::string remote_address;
    remote_address.assign(argv[1]);

    std::string port_str;
    port_str.assign(argv[2]);
    if (port_str.empty())
    {
        std::cout << "Port is empty" << std::endl;
        exit(-1);
    }
    auto server_port = std::stoi(argv[2], nullptr);

    std::string mode;
    mode.assign(argv[3]);
    if (mode != "send" && mode != "recv" && mode != "sendrecv")
    {
        std::cout << "Bad choice for mode.. Bye" << std::endl;
        exit(-1);
    }

    std::ostringstream oss;
    std::chrono::steady_clock::time_point timePoint = std::chrono::steady_clock::now();
    auto min = std::chrono::duration_cast<std::chrono::minutes>(timePoint.time_since_epoch());

    logger->SetLogLevel(LogLevel::INFO);

    std::default_random_engine engine(time(0));
    std::uniform_int_distribution distribution(1, 9);
    auto clientID = distribution(engine);
    logger->info << "ClientID: " << clientID << std::flush;

    // configure new stream callback
    auto wrapped_stream_callback = [](uint64_t client_id, uint64_t source_id, uint64_t source_ts, MediaType media_type)
    {
        logger->info << "[Sound]: New Sorcce" << source_id << std::flush;
        recvMedia = true;
        recvClientID = client_id;
        recvSourceID = source_id;
    };

    // Create media library.
    auto client = MediaClient{wrapped_stream_callback, logger};

    // SAH - client.init_transport(TransportType::QUIC, remote_address,
    // remote_port);

    shutDown = false;

    // setup port audio
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        assert(0);        // TODO
    }

    PaStreamParameters inputParameters;
    PaStreamParameters outputParameters;

    inputParameters.device = Pa_GetDefaultInputDevice();
    outputParameters.device = Pa_GetDefaultOutputDevice();

    assert(inputParameters.device != paNoDevice);
    assert(outputParameters.device != paNoDevice);

    assert(bytesPerSample == 4);

    inputParameters.channelCount = audio_channels;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.channelCount = audio_channels;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &audioStream, &inputParameters, &outputParameters, sample_rate, frames_per_buffer, paClipOff, NULL, NULL);
    if (err != paNoError)
    {
        logger->error << Pa_GetErrorText(err) << std::flush;
        assert(0);        // TODO
    }
    else
    {
        logger->info << "Audio device is open" << std::flush;
    }

    err = Pa_StartStream(audioStream);
    if (err != paNoError)
    {
        assert(0);        // TODO
    }

    std::vector<std::thread> threads;
    uint64_t stream_id{0};
    if (mode == "send")
    {
        MediaConfig config{};
        config.media_direction = MediaConfig::MediaDirection::sendonly;
        config.media_codec = MediaConfig::CodecType::opus;
        config.channels = 1;
        config.sample_rate = 48000;
        config.sample_type = AudioConfig::SampleType::Float32;
        stream_id = client.add_audio_stream(0x1000, 0x2000, 0x3000, config);
        threads.emplace_back(recordThreadFunc, &client, stream_id);
    }
    else if (mode == "recv")
    {
        MediaConfig config{};
        config.media_direction = MediaConfig::MediaDirection::recvonly;
        config.media_codec = MediaConfig::CodecType::opus;
        config.channels = 1;
        config.sample_rate = 48000;
        config.sample_type = AudioConfig::SampleType::Float32;
        stream_id = client.add_audio_stream(0x1000, 0x2000, 0x3000, config);
        threads.emplace_back(playThreadFunc, &client, stream_id);
        threads.at(0).detach();
    }
    else if (mode == "sendrecv")
    {
        MediaConfig config{};
        config.media_direction = MediaConfig::MediaDirection::sendrecv;
        config.media_codec = MediaConfig::CodecType::opus;
        config.channels = 1;
        config.sample_rate = 48000;
        config.sample_type = AudioConfig::SampleType::Float32;
        stream_id = client.add_audio_stream(0x1000, 0x2000, 0x3000, config);
        // start and play threads
        threads.emplace_back(recordThreadFunc, &client, stream_id);
        threads.emplace_back(playThreadFunc, &client, stream_id);
        threads.at(0).detach();
    }

    logger->info << "Starting Stream: " << stream_id << std::flush;
    int count = 0;
    while (!shutDown)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        count++;
        if (count > 50 * 1)
        {
            shutDown = false;
        }
    }
    logger->info << "Shutting down" << std::flush;

    // recordThread.join();
    // playThread.join();

    err = Pa_StopStream(audioStream);
    if (err != paNoError)
    {
        logger->error << "Failure to stop stream: " << Pa_GetErrorText(err) << std::flush;
    }

    if (outg_sound_file.is_open()) outg_sound_file.close();

    err = Pa_Terminate();
    if (err != paNoError)
    {
        logger->error << "Failure to shutdown portaudio: " << Pa_GetErrorText(err) << std::flush;
    }

    return err;
}

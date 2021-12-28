
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

#include "neo.hh"
#include "resampler.hh"
#include "logger.hh"

using namespace neo_media;

static bool shutDown;

static PaStream *audioStream;
static std::mutex audioReadMutex;
static std::mutex audioWriteMutex;

static bool recvMedia = false;
static uint64_t recvClientID;
static uint64_t recvSourceID;

static const unsigned int remote_port = 5004;
static const unsigned int sample_rate = 48000;
static const unsigned int frames_per_buffer = 10 * 48;        // 10 ms
static const unsigned int audio_channels = 1;
static const unsigned int bytesPerSample = 4;
static const Neo::audio_sample_type sample_type_neo =
    Neo::audio_sample_type::Float32;
static const double resample_ratio = 1.0;
static Resampler resampler;

static std::ofstream outg_sound_file;
static LoggerPointer logger = std::make_shared<Logger>("Sound", true);

void recordThreadFunc(Neo *neo)
{
    assert(audioStream);

    int buff_size = frames_per_buffer * bytesPerSample * audio_channels;
    char *zerobuff = (char *) malloc(buff_size);
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
        char *audioBuff = (char *) malloc(buff_size);
        if (audioBuff == nullptr)
        {
            logger->error << "Failed to allocate audio buffer" << std::flush;
            continue;
        }

        PaError err;
        {
            std::lock_guard<std::mutex> lock(audioReadMutex);
            err = Pa_ReadStream(audioStream, audioBuff, frames_per_buffer);
        }
        if (err)
        {
            logger->error << "Failed to read PA stream: "
                          << Pa_GetErrorText(err) << std::flush;
            continue;
        }

        if (memcmp(audioBuff, zerobuff, buff_size) == 0)
        {
            logger->debug << "0" << std::flush;
        }

        timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

        neo->sendAudio(audioBuff, buff_size, timestamp, sourceID);
        logger->debug << "-" << std::flush;

        if (outg_sound_file.is_open())
        {
            switch (sample_type_neo)
            {
                case Neo::audio_sample_type::Float32:
                {
                    float *fvalue = nullptr;
                    for (int i = 0; i < (buff_size / sizeof(float));
                         i += sizeof(float))
                    {
                        fvalue = (float *) &audioBuff[i];
                        outg_sound_file << *fvalue << ",";
                    }
                    break;
                }
                case Neo::audio_sample_type::PCMint16:
                {
                    uint16_t *ivalue = nullptr;
                    for (int i = 0; i < (buff_size / sizeof(uint16_t));
                         i += sizeof(uint16_t))
                    {
                        ivalue = (uint16_t *) audioBuff[i];
                        outg_sound_file << *ivalue << ",";
                    }
                    break;
                }
            }
        }
        free(audioBuff);
    }
    free(zerobuff);
}

void playThreadFunc(Neo *neo)
{
    assert(audioStream);

    std::chrono::steady_clock::time_point loop_time =
        std::chrono::steady_clock::now();

    while (!shutDown)
    {
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - loop_time);
        logger->debug << "{" << delta.count() << "}" << std::flush;
        loop_time = std::chrono::steady_clock::now();

        if (!recvMedia)
        {
            logger->debug << "Z" << std::flush;
            std::chrono::steady_clock::time_point sleep_time =
                std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            auto sleep_delta =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - sleep_time);
            logger->debug << "{S: " << sleep_delta.count() << "}" << std::flush;
            continue;
        }

        long playoutSpaceAvailable;
        {
            std::lock_guard<std::mutex> lock(audioWriteMutex);
            playoutSpaceAvailable = Pa_GetStreamWriteAvailable(audioStream);
            logger->debug << "[" << playoutSpaceAvailable << "]" << std::flush;
        }

        if (playoutSpaceAvailable < 1000 /* 21 ms */)
        {
            // not enought space to play, wait till later
            logger->debug << "$" << std::flush;
            std::chrono::steady_clock::time_point sleep_time =
                std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            auto sleep_delta =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - sleep_time);
            logger->debug << "{S: " << sleep_delta.count() << "}" << std::flush;
            continue;
        }

        int buff_size = frames_per_buffer * bytesPerSample * audio_channels;
        unsigned char *raw_data = nullptr;
        uint64_t timestamp;
        Packet *freePacket = nullptr;

        std::chrono::steady_clock::time_point get_audio =
            std::chrono::steady_clock::now();
        int recv_actual = neo->getAudio(recvClientID,
                                        recvSourceID,
                                        timestamp,
                                        &raw_data,
                                        buff_size,
                                        &freePacket);
        auto audio_delta = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - get_audio);
        logger->debug << "{A:" << audio_delta.count() << "}" << std::flush;

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
                logger->error
                    << "Pa_WriteStream (plc) failed: " << Pa_GetErrorText(err)
                    << std::flush;
                assert(0);        // TODO
            }
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        logger->debug << "+" << std::flush;
        {
            std::chrono::steady_clock::time_point start_write =
                std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(audioWriteMutex);

            if (resample_ratio != 1.0)
            {
                float *src = (float *) raw_data;
                long input_frames = recv_actual / (bytesPerSample);
                long resampled_output_frames = input_frames * resample_ratio;
                float *resampled = new float[resampled_output_frames];
                int ret = resampler.simple_resample(src,
                                                    input_frames,
                                                    resampled,
                                                    resampled_output_frames,
                                                    audio_channels,
                                                    resample_ratio);

                if (ret)
                {
                    logger->error << "Resample failed: " << src_strerror(ret)
                                  << std::flush;
                }
                else
                {
                    err = Pa_WriteStream(
                        audioStream,
                        (const void *) resampled,
                        resampled_output_frames / audio_channels);
                }

                delete[] resampled;
            }
            else
            {
                err = Pa_WriteStream(
                    audioStream,
                    raw_data,
                    recv_actual / (audio_channels * bytesPerSample));
                auto write_delta =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start_write);
                logger->debug << "{W: " << write_delta.count() << "}"
                              << std::flush;
            }
        }
        if (err)
        {
            logger->error << "Pa_WriteStream failed: " << Pa_GetErrorText(err)
                          << std::flush;
        }

        // hack until Packet is split into meta and data
        std::chrono::steady_clock::time_point free_time =
            std::chrono::steady_clock::now();
        if (freePacket != nullptr)
        {
            delete (Packet *) freePacket;
        }
        auto free_delta = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - free_time);
        logger->debug << "{F: " << free_delta.count() << "}" << std::flush;
    }
}

void streamCallBack(uint64_t clientID,
                    uint64_t sourceID,
                    uint64_t startTime,
                    Packet::MediaType type)
{
    // TODO: This will change to post decode only stream type at some point.
    if (type == Packet::MediaType::Opus)
    {
        recvMedia = true;
        recvClientID = clientID;
        recvSourceID = sourceID;
    }
}

int main(int argc, char *argv[])
{
#if defined(_WIN32)
    timeBeginPeriod(1);        // timerstonk - push minimum resolution down to 1
                               // ms
#endif
    if (argc < 4)
    {
        std::cout << "Must provide hostname of SFU on command line"
                  << std::endl;
        std::cout << "Usage: sound sfuHostname transport echo[true/false] "
                     "<optional: debug_mode> <optional: output_filename>"
                  << std::endl;
        std::cout << "transport -> q for quic, r for raw-udp" << std::endl;
        return 64;        // EX_USAGE in BSD for usage/parameter errors
    }

    std::string remote_address;
    remote_address.assign(argv[1]);

    std::string transport;
    transport.assign(argv[2]);

    // default to udp
    auto transport_type = NetTransport::Type::UDP;
    if (transport == "q")
    {
        logger->info << "Using Quic Transport" << std::flush;
        transport_type = NetTransport::Type::PICO_QUIC;
    }

    // SFU echo flag.
    bool echo = false;
    std::stringstream ss(argv[3]);
    ss >> std::boolalpha >> echo;

    std::ostringstream oss;
    std::chrono::steady_clock::time_point timePoint =
        std::chrono::steady_clock::now();
    auto min = std::chrono::duration_cast<std::chrono::minutes>(
        timePoint.time_since_epoch());

    // Debug mode / log level.
    bool debug = false;
    if (argc == 5)
    {
        bool echo = false;
        std::stringstream ss(argv[4]);
        ss >> std::boolalpha >> debug;
    }
    logger->SetLogLevel(debug ? LogLevel::DEBUG : LogLevel::INFO);

    if (argc == 6)
    {
        switch (sample_type_neo)
        {
            case Neo::audio_sample_type::Float32:
                oss << argv[5] << "-float32-" << audio_channels << "-"
                    << min.count() << ".csv";
                break;
            case Neo::audio_sample_type::PCMint16:
                oss << argv[5] << "-pcm16-" << audio_channels << "-"
                    << min.count() << ".csv";
                break;
        }

        outg_sound_file.open(oss.str());
        if (outg_sound_file.fail())
        {
            logger->error << "Unable to open sound file: " << oss.str()
                          << std::flush;
            return 73;        // EX_CANTCREATE in BSD for can't create output
                              // file
        }
    }

    std::default_random_engine engine(time(0));
    std::uniform_int_distribution distribution(1, 9);
    auto clientID = distribution(engine);
    logger->info << "ClientID: " << clientID << std::flush;

    Neo neo(logger);
    neo.init(remote_address,
             remote_port,        // network settings
             sample_rate,
             audio_channels,
             sample_type_neo,        // audio settings
             0,
             0,
             0,
             0,                                  // no video settings
             (Neo::video_pixel_format) 0,        // no video settings
             (Neo::video_pixel_format) 0,        // no video settings
             clientID,
             123456,        // conferenceID
             streamCallBack,
             transport_type,        // QUIC or UDP
             echo);

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
    assert(sample_type_neo == Neo::audio_sample_type::Float32);

    inputParameters.channelCount = audio_channels;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency =
        Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.channelCount = audio_channels;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency =
        Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&audioStream,
                        &inputParameters,
                        &outputParameters,
                        sample_rate,
                        frames_per_buffer,
                        paClipOff,
                        NULL,
                        NULL);
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

    std::thread recordThread(recordThreadFunc, &neo);
    std::thread playThread(playThreadFunc, &neo);
    playThread.detach();

    logger->info << "Starting" << std::flush;
    int count = 0;
    while (!shutDown)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        count++;
        if (count > 50 * 1)
        {
            shutDown = true;
        }
    }
    logger->info << "Shutting down" << std::flush;

    recordThread.join();
    playThread.join();

    err = Pa_StopStream(audioStream);
    if (err != paNoError)
    {
        logger->error << "Failure to stop stream: " << Pa_GetErrorText(err)
                      << std::flush;
    }

    if (outg_sound_file.is_open()) outg_sound_file.close();

    err = Pa_Terminate();
    if (err != paNoError)
    {
        logger->error << "Failure to shutdown portaudio: "
                      << Pa_GetErrorText(err) << std::flush;
    }

    return err;
}

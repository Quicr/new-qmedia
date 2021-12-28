#include <iostream>
#include <chrono>
#include <thread>
#include <cassert>
#include <cmath>

#include "neo_media_client.hh"
#include "neo.hh"

using namespace std::chrono_literals;

std::uint64_t source_id = 0;              // wait to receive something non-zero
std::uint64_t send_source_id = 37;        // random choice in send direction

void source_callback(std::uint64_t cid,
                     std::uint64_t sid,
                     std::uint64_t ts,
                     int source_type)
{
    source_id = sid;
    std::cout << std::endl
              << "Got New Source:" << sid << " Client:" << cid << " Time:" << ts
              << " Type:" << source_type << std::endl;
}

int main(int argc, char **argv)
{
    std::uint64_t client_id = 5;            // made it up
    std::uint64_t conference_id = 1;        // made it up

    std::uint32_t enc_format = 0;        // encode format: 0=NV12
    std::uint32_t dec_format = 1;        // decode format: 1=I420

    std::size_t image_width = 1280;
    std::size_t image_height = 720;
    std::size_t image_y_size = image_width * image_height;
    std::size_t image_uv_size = image_y_size >> 2;        // YUV420
    std::size_t image_size = image_y_size + image_uv_size * 2;
    char *image = static_cast<char *>(malloc(image_size));
    assert(image);

    // Fill image with color gradients.
    // 16M YUV combos mapped to 1M pixels.
    // 8 bits of Y in MSBs + 6 bits of U/V in LSBs = 20 bits total = 1M.

    for (int i = 0; i < image_y_size; i++)
    {
        image[i] = (i >> 12 & 0xff) + 16;        // Y in bits 12-19
    }
    for (int i = 0; i < image_uv_size; i++)
    {
        image[image_y_size + i * 2 + 0] = (i >> 6 & 0x3f) << 2;        // U in
                                                                       // bits
                                                                       // 6-11
        image[image_y_size + i * 2 + 1] = (i >> 0 & 0x3f) << 2;        // V in
                                                                       // bits
                                                                       // 0-5
    }

    NeoMediaInstance neo = Init(
        "127.0.0.1",        // server name/IP
        5004,               // UDP port
        48000,              // 48kHz audio sample rate
        2,                  // audio channels
        0,                  // 0=Neo::PCMfloat32=0, 1=Neo::PCMint16
        image_width,
        image_height,
        30,                              // frame rate
        200000,                          // bitrate
        enc_format,                      // encode format: 0=NV12
        dec_format,                      // decode format: 1=I420
        client_id,                       // my (sender) client id
        conference_id,                   // my (sender) conference id
        source_callback,                 // callback when received a new source
        NetTransport::Type::UDP,         // UDP or QUIC
        [](const char *message) {        // Logs
            std::cout << message << std::endl;
        },
        false);        // echo

    setLoopbackMode(neo, 3);

    // Send frames.
    std::thread sendThread(
        [neo, image, image_size, image_width, image_height, enc_format]() {
            while (true)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(33));
                std::uint64_t timestamp =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
                std::cerr << " S ";        // << sourceRecordTime << "ns " <<
                                           // image_size << "bytes" <<
                                           // std::endl;
                sendVideoFrame(neo,
                               image,
                               image_size,
                               image_width,
                               image_height,
                               image_width,
                               image_width,
                               image_width * image_height,
                               0,
                               enc_format,
                               timestamp,
                               send_source_id);
                image[0] = image[0] + 1;        // Embed a frame counter to
                                                // check on receive
            }
        });

    // Get frames.
    std::thread recvThread([neo,
                            client_id,
                            image,
                            image_size,
                            image_width,
                            image_height,
                            dec_format]() {
        while (true)
        {
            std::cerr << " R ";        // << ts << " " << received_image_size <<
                                       // std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            if (source_id == 0) continue;
            unsigned char *buffer = nullptr;
            std::uint64_t ts = 0;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t format = dec_format;        // I420
            auto received_image_size = getVideoFrame(
                neo, client_id, source_id, ts, width, height, format, &buffer);

            // std::cerr << " w:" << width << " iw:" << image_width
            //          << " h:" << height << " ih:" << image_height
            //          << " f:" << format << " df:" << dec_format << std::endl;
            assert(received_image_size == image_size);
            assert(width == image_width);
            assert(height == image_height);
            assert(format == dec_format);
            assert(buffer);

            // Enable assert below if testing Raw video with echoSFU and no
            // concealment. Must explicitly disable AV1 and decoder concealment
            // for this assert to pass. assert(buffer[0] == 0x80 ||
            // memcmp(buffer, image, image_size) == 0);
            std::cerr << " " << (int) (buffer[0]) << " ";        // log frame
                                                                 // counter

            /* PSNR calc to test sanity (30-50 is believable) and quality
            (>40=great) std::int64_t sq_err = 1; for (int i = 0; i < image_size;
            ++i) { int diff = image[i] - buffer[i]; sq_err += diff * diff;
            }
            double psnr = 10.0 * log10(255.0*255.0*image_size*8/12/sq_err);
            if (psnr>100.0) psnr=100.0;
            std::cerr << " PSNR=" << psnr << " ";
            */
            std::cerr << " r ";        // << ts << " " << received_image_size <<
                                       // std::endl;
        }
    });

    // Main thread.
    while (true)
    {
    }
}

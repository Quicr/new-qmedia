#pragma once

#include <list>
#include <vector>
#include <mutex>

namespace neo_media
{
/**
 * Class to match application buffer requests
 */
class fullFill
{
public:
    fullFill();
    ~fullFill() = default;
    unsigned int getTotalInBuffers();
    void addBuffer(const uint8_t *buffer,
                   unsigned int length,
                   std::uint64_t timestamp);
    bool fill(std::vector<uint8_t> &fill_buffer,
              unsigned int fill_length,
              std::uint64_t &timestamp);

    // Buffer to satisy application pull rates
    using fill_buffers = std::list<std::pair<std::vector<uint8_t>, uint64_t>>;
    fill_buffers buffers;
    unsigned int read_front;
    std::mutex buff_mutex;

    [[nodiscard]] uint64_t calculate_timestamp(unsigned int read_front,
                                               uint64_t timestamp) const;
    uint32_t sample_divisor = 1 * sizeof(float);        // per_sample_divisor is
                                                        // to translate the
                                                        // residual bytes in
                                                        // buffer into samples
                                                        // that can be
                                                        // translated to
                                                        // microseconds
};
}        // namespace neo_media

#pragma once

#include <string>
#include <vector>
#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace qmedia
{
///
/// Utility
///

std::vector<uint8_t> hash(const std::vector<uint8_t> &data)
{
    auto algo = EVP_sha256();
    auto hash_size = EVP_MD_size(algo);
    auto md = std::vector<uint8_t>(hash_size);
    unsigned int size = 0;
    if (1 !=
        EVP_Digest(data.data(), data.size(), md.data(), &size, algo, nullptr))
    {
        throw std::runtime_error("EVP_Digest error");
    }

    return md;
}

struct QuicrName
{
    static constexpr auto base = "quicr://";

    static std::string name_for_client(uint64_t domain,
                                       uint64_t conference,
                                       uint64_t client_id)
    {
        return name_for_conference(domain, conference) +
               std::to_string(client_id);
    }

    static std::string name_for_conference(uint64_t domain, uint64_t conference)
    {
        return base + std::to_string(domain) + "/" +
               std::to_string(conference) + "/";
    }
};

}        // namespace qmedia
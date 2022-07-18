#include <doctest/doctest.h>
#include "../src/transport_manager.hh"

using namespace neo_media;

static bytes from_hex(const std::string &hex)
{
    if (hex.length() % 2 == 1)
    {
        throw std::invalid_argument("Odd-length hex string");
    }

    auto len = int(hex.length() / 2);
    auto out = bytes(len);
    for (int i = 0; i < len; i += 1)
    {
        auto byte = hex.substr(2 * i, 2);
        out[i] = static_cast<uint8_t>(strtol(byte.c_str(), nullptr, 16));
    }

    return out;
}

#if 0
TEST_CASE("Sframe protect/unprotect")
{
    auto client = ClientTransportManager(
        NetTransport::Type::UDP, "127.0.0.1", 10000);
    int64_t epoch = 1001;
    client.setCryptoKey(epoch, bytes(8, uint8_t(epoch)));
    const auto plaintext = from_hex("00010203");
    auto encrypted = client.protect(plaintext);
    auto decrypted = client.unprotect(encrypted);
    CHECK(plaintext == decrypted);
}
#endif
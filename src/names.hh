#pragma once

#include <string>
#include <vector>

namespace qmedia
{

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
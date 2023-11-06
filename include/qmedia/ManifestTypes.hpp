#pragma once

#include <qname>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

using json = nlohmann::json;

namespace qmedia::manifest
{

struct Manifest;

struct Profile
{
    std::string qualityProfile;
    std::string url;
    quicr::Namespace quicrNamespace;
    std::vector<uint8_t> priorities;
    std::optional<uint16_t> expiry = 0;
    friend bool operator==(const Profile& lhs, const Profile& rhs);
};

struct ProfileSet
{
    std::string type;
    std::vector<Profile> profiles;

    friend bool operator==(const ProfileSet& lhs, const ProfileSet& rhs);
};

struct MediaStream
{
    std::string mediaType;
    std::string sourceName;
    std::string sourceId;
    std::string label;
    ProfileSet profileSet;

    friend bool operator==(const MediaStream& lhs, const MediaStream& rhs);
};

struct Manifest
{
    std::vector<MediaStream> subscriptions;
    std::vector<MediaStream> publications;
    std::vector<std::string> url_templates; // hacking for moq
    std::vector<std::string> urls; // hacking for moq

    friend bool operator==(const Manifest& lhs, const Manifest& rhs);
};

// A custom parser is required here so that the templates in `urlTemplates` can
// be used to convert the `quicrNamespaceUrl` values in the profiles to
// `quicr::Namespace` values.
void from_json(const nlohmann::json& j, Manifest& manifest);

}        // namespace qmedia::manifest

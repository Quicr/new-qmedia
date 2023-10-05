#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

using json = nlohmann::json;

namespace qmedia::manifest
{

struct Profile
{
    std::string qualityProfile;
    std::string quicrNamespaceUrl;
    std::vector<uint8_t> priorities;
    std::optional<uint16_t> expiry = 0;

    friend bool operator==(const Profile& lhs, const Profile& rhs);
};

// A custom parser is required here because the `priorities` and `expiry` fields
// are optional.  The `to_json` method is a stub, provided only to allow for the
// use of macros to define JSON formatting for higher-level structs.
void to_json(nlohmann::json& j, const Profile& profile);
void from_json(const nlohmann::json& j, Profile& profile);

struct ProfileSet
{
    std::string type;
    std::vector<Profile> profiles;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProfileSet, type, profiles)
    friend bool operator==(const ProfileSet& lhs, const ProfileSet& rhs);
};

struct MediaStream
{
    std::string mediaType;
    std::string sourceName;
    std::string sourceId;
    std::string label;
    ProfileSet profileSet;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MediaStream, mediaType, sourceName, sourceId, label, profileSet)
    friend bool operator==(const MediaStream& lhs, const MediaStream& rhs);
};

struct Manifest
{
    std::vector<std::string> urlTemplates;
    std::vector<MediaStream> subscriptions;
    std::vector<MediaStream> publications;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Manifest, urlTemplates, subscriptions, publications)
    friend bool operator==(const Manifest& lhs, const Manifest& rhs);
};

}        // namespace qmedia::manifest

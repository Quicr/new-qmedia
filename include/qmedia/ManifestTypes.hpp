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
    std::string quicrNamespaceURL;
};

void to_json(json& j, const Profile& profile);
void from_json(const json& j, Profile& profile);

struct ProfileSet
{
    std::string type;
    std::vector<Profile> profiles;
};

void to_json(json& j, const ProfileSet& profile);
void from_json(const json& j, ProfileSet& profile);

struct Subscription
{
    std::string mediaType;
    std::string sourceName;
    std::string sourceID;
    std::string label;
    ProfileSet profileSet;
};

void to_json(json& j, const Subscription& profile);
void from_json(const json& j, Subscription& profile);
}        // namespace qmedia::manifest

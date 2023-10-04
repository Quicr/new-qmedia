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
    uint16_t expiry;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Profile, qualityProfile, quicrNamespaceUrl, priorities, expiry)
};

struct ProfileSet
{
    std::string type;
    std::vector<Profile> profiles;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProfileSet, type, profiles)
};

struct Subscription
{
    std::string mediaType;
    std::string sourceName;
    std::string sourceId;
    std::string label;
    ProfileSet profileSet;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Subscription, mediaType, sourceName, sourceId, label, profileSet)
};

struct Publication {
  std::string sourceId;
  ProfileSet profileSet;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Publication, sourceId, profileSet)
};

struct Manifest {
  std::vector<std::string> urlTemplates;
  std::vector<Subscription> subscriptions;
  std::vector<Publication> publications;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Manifest, urlTemplates, subscriptions, publications)
};

}

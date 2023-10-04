#include "qmedia/ManifestTypes.hpp"

namespace qmedia::manifest
{
void to_json(json& j, const Profile& profile)
{
    j = {
        {"qualityProfile", profile.qualityProfile},
        {"quicrNamespaceUrl", profile.quicrNamespaceURL},
    };
}

void from_json(const json& j, Profile& profile)
{
    j.at("qualityProfile").get_to(profile.qualityProfile);
    j.at("quicrNamespaceUrl").get_to(profile.quicrNamespaceURL);
}

void to_json(json& j, const ProfileSet& profile_set)
{
    j = {
        {"type", profile_set.type},
        {"profiles", profile_set.profiles},
    };
}

void from_json(const json& j, ProfileSet& profile_set)
{
    j.at("type").get_to(profile_set.type);
    j.at("profiles").get_to(profile_set.profiles);
}

void to_json(json& j, const Subscription& subscription)
{
    j = {
        {"mediaType", subscription.mediaType},
        {"sourceName", subscription.sourceName},
        {"sourceId", subscription.sourceID},
        {"label", subscription.label},
        {"profileSet", subscription.profileSet},
    };
}

void from_json(const json& j, Subscription& subscription)
{
    j.at("mediaType").get_to(subscription.mediaType);
    j.at("sourceName").get_to(subscription.sourceName);
    j.at("sourceId").get_to(subscription.sourceID);
    j.at("label").get_to(subscription.label);
    j.at("profileSet").get_to(subscription.profileSet);
}
}        // namespace qmedia::manifest

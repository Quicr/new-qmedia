#include <qmedia/ManifestTypes.hpp>

namespace qmedia::manifest
{

void to_json(nlohmann::json& /* j */, const Profile& /* profile */) {
    throw std::runtime_error("JSON serialization not implemented");
}

void from_json(const nlohmann::json& j, Profile& profile)
{
    j.at("qualityProfile").get_to(profile.qualityProfile);
    j.at("quicrNamespaceUrl").get_to(profile.quicrNamespaceUrl);

    if (j.contains("priorities")) {
      j.at("priorities").get_to(profile.priorities);
    }

    if (j.contains("expiry")) {
      auto expiry = uint16_t(0);
      j.at("expiry").get_to(expiry);
      profile.expiry = expiry;
    }
}

bool operator==(const Profile& lhs, const Profile& rhs)
{
    return lhs.qualityProfile == rhs.qualityProfile && lhs.quicrNamespaceUrl == rhs.quicrNamespaceUrl &&
           lhs.priorities == rhs.priorities && lhs.expiry == rhs.expiry;
}

bool operator==(const ProfileSet& lhs, const ProfileSet& rhs)
{
    return lhs.type == rhs.type && lhs.profiles == rhs.profiles;
}

bool operator==(const MediaStream& lhs, const MediaStream& rhs)
{
    return lhs.mediaType == rhs.mediaType && lhs.sourceName == rhs.sourceName && lhs.sourceId == rhs.sourceId &&
           lhs.label == rhs.label && lhs.profileSet == rhs.profileSet;
}

bool operator==(const Manifest& lhs, const Manifest& rhs)
{
    return lhs.urlTemplates == rhs.urlTemplates && lhs.subscriptions == rhs.subscriptions &&
           lhs.publications == rhs.publications;
}

}        // namespace qmedia::manifest

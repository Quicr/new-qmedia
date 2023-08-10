#include "ProfileSet.hpp"

namespace ns {
    void to_json(json& json, const ProfileSet& profileSet) {
        j = json{ { "type", profileSet.type }, { "profiles", profileSet.profiles } };
    }

    void from_json(const json& json, ProfileSet& profileSet) {
        profileSet.type = j.at("type").get<std::string>();
        profileSet.profiles = j.at("profiles").get<Profile[]>();
    }

    void to_json(json& json, const Profile& profile) {
        j = json{ { "qualityProfile", profile.qualityProfile }, { "expiry", profile.timeToLive }, { "priorities", profile.priorities }, { "quicrNamespaceUrl", profile.quicrNamespaceUrl } };
    }

    void from_json(const json& json, Profile& profile) {
        profile.qualityProfile = j.at("qualityProfile").get<std::string>();
        profile.timeToLive = j.at("expiry").get<int>();
        profile.priorities = j.at("priorities").get<int[]>();
        profile.quicrNamespaceUrl = j.at("quicrNamespaceUrl").get<std::string>();
    }
}
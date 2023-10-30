#include <qmedia/ManifestTypes.hpp>

#include <UrlEncoder.h>

namespace qmedia::manifest
{

struct ParseContext
{
    UrlEncoder url_encoder;
};

void from_json(const ParseContext& ctx, const nlohmann::json& j, Profile& profile)
{
    j.at("qualityProfile").get_to(profile.qualityProfile);

    const auto namespace_url = j.at("quicrNamespaceUrl").get<std::string>();
    profile.quicrNamespace = ctx.url_encoder.EncodeUrl(namespace_url);

    if (j.contains("priorities"))
    {
        j.at("priorities").get_to(profile.priorities);
    }

    if (j.contains("expiry"))
    {
        auto expiry = uint16_t(0);
        j.at("expiry").get_to(expiry);
        profile.expiry = expiry;
    }
}

void from_json(const ParseContext& ctx, const nlohmann::json& j, ProfileSet& profile_set)
{
    j.at("type").get_to(profile_set.type);
    for (const auto& j : j.at("profiles"))
    {
        auto profile = Profile{};
        from_json(ctx, j, profile);
        profile_set.profiles.push_back(std::move(profile));
    }
}

void from_json(const ParseContext& ctx, const nlohmann::json& j, MediaStream& media_stream)
{
    j.at("mediaType").get_to(media_stream.mediaType);
    j.at("sourceName").get_to(media_stream.sourceName);
    j.at("sourceId").get_to(media_stream.sourceId);
    j.at("label").get_to(media_stream.label);
    from_json(ctx, j.at("profileSet"), media_stream.profileSet);
}

void from_json(const nlohmann::json& j, Manifest& manifest)
{
    auto ctx = ParseContext{};
    const auto url_templates = j.at("urlTemplates").get<std::vector<std::string>>();
    for (const auto& url_template : url_templates)
    {
        ctx.url_encoder.AddTemplate(url_template, true);
    }

    for (const auto& j : j.at("subscriptions"))
    {
        auto media_stream = MediaStream{};
        from_json(ctx, j, media_stream);
        manifest.subscriptions.push_back(media_stream);
    }

    for (const auto& j : j.at("publications"))
    {
        auto media_stream = MediaStream{};
        from_json(ctx, j, media_stream);
        manifest.publications.push_back(media_stream);
    }
}

bool operator==(const Profile& lhs, const Profile& rhs)
{
    return lhs.qualityProfile == rhs.qualityProfile && lhs.quicrNamespace == rhs.quicrNamespace &&
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
    return lhs.subscriptions == rhs.subscriptions && lhs.publications == rhs.publications;
}

}        // namespace qmedia::manifest

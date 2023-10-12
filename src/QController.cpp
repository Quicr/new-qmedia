#include "qmedia/QController.hpp"
#include "qmedia/QuicrDelegates.hpp"
#include "qmedia/ManifestTypes.hpp"

#include <quicr/hex_endec.h>

#include <iostream>
#include <sstream>

namespace qmedia
{

QController::QController(std::shared_ptr<SubscriberDelegate> subscriberDelegate,
                         std::shared_ptr<PublisherDelegate> publisherDelegate,
                         const cantina::LoggerPointer& logger) :
    logger(std::make_shared<cantina::Logger>("QCTRL", logger)),
    subscriberDelegate(std::move(subscriberDelegate)),
    publisherDelegate(std::move(publisherDelegate)),
    stop(false),
    closed(false)
{
    LOGGER_DEBUG(logger, "QController started...");

    // quicr://webex.cisco.com/conference/1/mediaType/192/endpoint/2
    //   org, app,   conf, media, endpoint,     group, object
    //   24,   8,      24,     8,       16,        32,     16
    // 000001  01   000001     c0      0002  / 00000000,  0000
    // pen - private enterprise number (24 bits)
    // sub_pen - sub-private enterprise number (8 bits)
    //
    // SAH - fixme - rename "mediaType" -> "mediatype"
    // SAH - fixme - rename "conference" -> "conferences"
}

QController::~QController()
{
    disconnect();
}

int QController::connect(const std::string remoteAddress,
                         std::uint16_t remotePort,
                         quicr::RelayInfo::Protocol protocol,
                         const qtransport::TransportConfig& config)
{
    quicr::RelayInfo relayInfo = {
        .hostname = remoteAddress.c_str(),
        .port = remotePort,
        .proto = protocol,
    };

    client_session = std::make_unique<quicr::Client>(relayInfo, config, logger);

    try
    {
        if (!client_session->connect()) return -1;
    }
    catch (const std::exception& e)
    {
        LOGGER_ERROR(logger, "Failed to connect to " << remoteAddress << ": " << e.what());
        return -1;
    }

    // check to see if there is a timer thread...
    keepaliveThread = std::thread(&QController::periodicResubscribe, this, 5);
    return 0;
}

int QController::disconnect()
{
    if (closed)
    {
        return -1;
    }

    LOGGER_DEBUG(logger, "Disconnecting client session...");

    stop = true;

    if (keepaliveThread.joinable())
    {
        keepaliveThread.join();
    }

    if (client_session)
    {
        client_session->disconnect();
        client_session.reset();
    }

    closed = true;

    LOGGER_INFO(logger, "Disconnected client session");

    return 0;
}

void QController::periodicResubscribe(const unsigned int seconds)
{
    LOGGER_INFO(logger, "Started keep-alive thread");

    std::chrono::system_clock::time_point timeout = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
    while (!stop)
    {
        std::chrono::duration<int, std::milli> timespan(100);        // sleep duration in mills
        std::this_thread::sleep_for(timespan);
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

        if (now >= timeout && !stop)
        {
            LOGGER_DEBUG(logger, "Resubscribing...");
            const std::lock_guard<std::mutex> _(subsMutex);
            for (auto const& [key, delegate] : subscriptionsMap)
            {
                delegate->subscribe(client_session, quicr::SubscribeIntent::sync_up, "", true, "", {});
            }
            timeout = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
        }
    }

    LOGGER_INFO(logger, "Closed keep-alive thread");
}

void QController::removeSubscriptions()
{
    LOGGER_DEBUG(logger, "Unsubscribing from all subscriptions...");

    const std::lock_guard<std::mutex> _(subsMutex);
    for (auto const& [ns, delegate] : subscriptionsMap)
    {
        delegate->unsubscribe(client_session, "", "");
        LOGGER_DEBUG(logger, "Unsubscribed " << ns);
    }

    LOGGER_INFO(logger, "Unsubscribed from all subscriptions");
}

void QController::publishNamedObject(const quicr::Namespace& quicrNamespace,
                                     std::uint8_t* data,
                                     std::size_t len,
                                     bool groupFlag,
                                     bool reliableTransport)
{
    const std::lock_guard<std::mutex> _(pubsMutex);

    if (!publicationsMap.contains(quicrNamespace)) return;

    if (auto publicationDelegate = publicationsMap.at(quicrNamespace))
    {
        publicationDelegate->publishNamedObject(data, len, groupFlag, reliableTransport);
    }
}

/*
 * For Test Only
 */
void QController::publishNamedObjectTest(std::uint8_t* data, std::size_t len, bool groupFlag, bool reliableTransport)
{
    const std::lock_guard<std::mutex> _(pubsMutex);
    if (publicationsMap.empty()) return;

    auto publicationDelegate = publicationsMap.begin()->second;
    publicationDelegate->publishNamedObject(data, len, groupFlag, reliableTransport);
}

/*===========================================================================*/
// Quicr Delegates
/*===========================================================================*/

std::shared_ptr<SubscriptionDelegate> QController::findQuicrSubscriptionDelegate(const quicr::Namespace& quicrNamespace)
{
    std::lock_guard<std::mutex> _(subsMutex);
    if (subscriptionsMap.contains(quicrNamespace))
    {
        return subscriptionsMap[quicrNamespace];
    }
    return nullptr;
}

std::shared_ptr<PublicationDelegate> QController::findQuicrPublicationDelegate(const quicr::Namespace& quicrNamespace)
{
    std::lock_guard<std::mutex> _(pubsMutex);
    if (publicationsMap.contains(quicrNamespace))
    {
        return publicationsMap[quicrNamespace];
    }
    return nullptr;
}

/*===========================================================================*/
// QController Delegates
/*===========================================================================*/

std::shared_ptr<SubscriptionDelegate> QController::getSubscriptionDelegate(const quicr::Namespace& quicrNamespace,
                                                                           const std::string& qualityProfile)
{
    if (!subscriberDelegate)
    {
        LOGGER_ERROR(logger, "Subscription delegate doesn't exist for " << quicrNamespace);
        return nullptr;
    }

    std::lock_guard<std::mutex> _(qSubsMutex);
    if (!subscriptionsMap.contains(quicrNamespace))
    {
        subscriptionsMap[quicrNamespace] = subscriberDelegate->allocateSubByNamespace(
            quicrNamespace, qualityProfile, logger);
    }

    return subscriptionsMap[quicrNamespace];
}

std::shared_ptr<PublicationDelegate> QController::getPublicationDelegate(const quicr::Namespace& quicrNamespace,
                                                                         const std::string& sourceID,
                                                                         const std::vector<std::uint8_t>& priorities,
                                                                         std::uint16_t expiry,
                                                                         const std::string& qualityProfile)
{
    if (!publisherDelegate)
    {
        LOGGER_ERROR(logger, "Publication delegate doesn't exist for " << quicrNamespace);
        return nullptr;
    }

    std::lock_guard<std::mutex> _(qPubsMutex);
    if (!publicationsMap.contains(quicrNamespace))
    {
        auto delegate = publisherDelegate->allocatePubByNamespace(
            quicrNamespace, sourceID, priorities, expiry, qualityProfile, logger);

        delegate->setClientSession(client_session);
        publicationsMap[quicrNamespace] = delegate;
    }

    return publicationsMap[quicrNamespace];
}

void QController::processURLTemplates(const std::vector<std::string>& urlTemplates)
{
    LOGGER_DEBUG(logger, "Processing URL templates...");
    for (auto& urlTemplate : urlTemplates)
    {
        encoder.AddTemplate(urlTemplate, true);
    }
    LOGGER_INFO(logger, "Finished processing templates!");
}

void QController::processSubscriptions(const std::vector<manifest::MediaStream>& subscriptions)
{
    LOGGER_DEBUG(logger, "Processing subscriptions...");
    for (auto& subscription : subscriptions)
    {
        for (auto& profile : subscription.profileSet.profiles)
        {
            quicr::Namespace quicrNamespace = encoder.EncodeUrl(profile.quicrNamespaceUrl);

            auto delegate = getSubscriptionDelegate(quicrNamespace, profile.qualityProfile);
            if (!delegate)
            {
                LOGGER_WARNING(logger, "Unable to allocate subscription delegate.");
                continue;
            }

            int update_error = delegate->update(subscription.label, profile.qualityProfile);
            if (update_error == 0)
            {
                LOGGER_INFO(logger, "Updated subscription " << quicrNamespace);
                continue;
            }

            bool reliable = false;
            int prepare_error = delegate->prepare(subscription.label, profile.qualityProfile, reliable);

            if (prepare_error != 0)
            {
                LOGGER_ERROR(logger, "Error preparing subscription: " << prepare_error);
                continue;
            }

            quicr::bytes e2eToken;
            delegate->subscribe(client_session, quicr::SubscribeIntent::sync_up, "", reliable, "", std::move(e2eToken));

            // If singleordered, and we've successfully processed 1 delegate, break.
            if (subscription.profileSet.type == "singleordered") break;
        }
    }

    LOGGER_INFO(logger, "Finished processing subscriptions!");
}

void QController::processPublications(const std::vector<manifest::MediaStream>& publications)
{
    LOGGER_DEBUG(logger, "Processing publications...");
    for (auto& publication : publications)
    {
        for (auto& profile : publication.profileSet.profiles)
        {
            const auto& quicrNamespace = encoder.EncodeUrl(profile.quicrNamespaceUrl);

            std::uint16_t expiry = profile.expiry.has_value() ? profile.expiry.value() : 0;
            auto delegate = getPublicationDelegate(
                quicrNamespace, publication.sourceId, profile.priorities, expiry, profile.qualityProfile);
            if (!delegate)
            {
                LOGGER_ERROR(logger, "Failed to create publication delegate: " << quicrNamespace);
                continue;
            }

            // Notify client to prepare for incoming media
            bool reliable = false;
            int prepare_error = delegate->prepare(profile.qualityProfile, reliable);
            if (prepare_error != 0)
            {
                LOGGER_WARNING(logger, "Preparing publication \"" << quicrNamespace << "\" failed: " << prepare_error);
                continue;
            }

            delegate->publishIntent(reliable, {});

            // If singleordered, and we've successfully processed 1 delegate, break.
            if (publication.profileSet.type == "singleordered") break;
        }
    }

    LOGGER_INFO(logger, "Finished processing publications!");
}

void QController::updateManifest(const manifest::Manifest& manifest_obj)
{
    LOGGER_DEBUG(logger, "Importing manifest...");

    processURLTemplates(manifest_obj.urlTemplates);
    processSubscriptions(manifest_obj.subscriptions);
    processPublications(manifest_obj.publications);

    LOGGER_INFO(logger, "Finished importing manifest!");
}

}        // namespace qmedia

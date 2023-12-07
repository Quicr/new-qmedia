#include "qmedia/QController.hpp"
#include "qmedia/QuicrDelegates.hpp"
#include "qmedia/ManifestTypes.hpp"

#include <quicr/hex_endec.h>

#include <iostream>
#include <sstream>

namespace qmedia
{

QController::QController(std::shared_ptr<QSubscriberDelegate> qSubscriberDelegate,
                         std::shared_ptr<QPublisherDelegate> qPublisherDelegate,
                         const cantina::LoggerPointer& logger) :
    logger(std::make_shared<cantina::Logger>("QCTRL", logger)),
    qSubscriberDelegate(std::move(qSubscriberDelegate)),
    qPublisherDelegate(std::move(qPublisherDelegate)),
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

    if (!client_session->connect()) return -1;

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
    }

    closed = true;

    LOGGER_INFO(logger, "Disconnected client session");

    return 0;
}

void QController::close()
{
    disconnect();
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
            for (auto const& [key, quicrSubDelegate] : quicrSubscriptionsMap)
            {
                quicrSubDelegate->subscribe(client_session);
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
    for (auto const& [key, quicrSubDelegate] : quicrSubscriptionsMap)
    {
        quicrSubDelegate->unsubscribe(client_session);
        LOGGER_DEBUG(logger, "Unsubscribed " << key);
    }

    LOGGER_INFO(logger, "Unsubscribed from all subscriptions");
}

void QController::publishNamedObject(const quicr::Namespace& quicrNamespace,
                                     const std::uint8_t* data,
                                     std::size_t len,
                                     bool groupFlag)
{
    const std::lock_guard<std::mutex> _(pubsMutex);

    if (!quicrPublicationsMap.contains(quicrNamespace)) return;

    if (auto publicationDelegate = quicrPublicationsMap.at(quicrNamespace))
    {
        publicationDelegate->publishNamedObject(this->client_session, data, len, groupFlag);
    }
}

/*
 * For Test Only
 */
void QController::publishNamedObjectTest(std::uint8_t* data, std::size_t len, bool groupFlag)
{
    const std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.empty()) return;

    auto publicationDelegate = quicrPublicationsMap.begin()->second;
    publicationDelegate->publishNamedObject(this->client_session, data, len, groupFlag);
}

/*===========================================================================*/
// Quicr Delegates
/*===========================================================================*/

std::shared_ptr<SubscriptionDelegate> QController::findQuicrSubscriptionDelegate(const quicr::Namespace& quicrNamespace)
{
    std::lock_guard<std::mutex> _(subsMutex);
    if (quicrSubscriptionsMap.contains(quicrNamespace))
    {
        return quicrSubscriptionsMap[quicrNamespace];
    }
    return nullptr;
}

std::shared_ptr<SubscriptionDelegate>
QController::createQuicrSubscriptionDelegate(const std::string& sourceId,
                                             const quicr::Namespace& quicrNamespace,
                                             const quicr::SubscribeIntent intent,
                                             const std::string& originUrl,
                                             const bool useReliableTransport,
                                             const std::string& authToken,
                                             quicr::bytes&& e2eToken,
                                             std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate)
{
    std::lock_guard<std::mutex> _(subsMutex);
    if (quicrSubscriptionsMap.contains(quicrNamespace))
    {
        LOGGER_ERROR(logger, "Quicr Subscription delegate for \"" << quicrNamespace << "\" already exists!");
        return nullptr;
    }

    const quicr::TransportMode transport_mode = useReliableTransport ? quicr::TransportMode::ReliablePerGroup
                                                                     : quicr::TransportMode::Unreliable;
    quicrSubscriptionsMap[quicrNamespace] = SubscriptionDelegate::create(sourceId,
                                                                         quicrNamespace,
                                                                         intent,
                                                                         transport_mode,
                                                                         originUrl,
                                                                         authToken,
                                                                         std::move(e2eToken),
                                                                         std::move(qDelegate),
                                                                         logger);
    return quicrSubscriptionsMap[quicrNamespace];
}

std::shared_ptr<PublicationDelegate> QController::findQuicrPublicationDelegate(const quicr::Namespace& quicrNamespace)
{
    std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.contains(quicrNamespace))
    {
        return quicrPublicationsMap[quicrNamespace]->getptr();
    }
    return nullptr;
}

std::shared_ptr<PublicationDelegate>
QController::createQuicrPublicationDelegate(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                                            const std::string& sourceId,
                                            const quicr::Namespace& quicrNamespace,
                                            const std::string& originUrl,
                                            const std::string& authToken,
                                            quicr::bytes&& payload,
                                            const std::vector<std::uint8_t>& priority,
                                            std::uint16_t expiry,
                                            bool reliableTransport)
{
    const quicr::TransportMode transport_mode = reliableTransport ? quicr::TransportMode::ReliablePerGroup
                                                                  : quicr::TransportMode::Unreliable;

    std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.contains(quicrNamespace))
    {
        LOGGER_ERROR(logger, "Quicr Publication delegate for \"" << quicrNamespace << "\" already exists!");
        return nullptr;
    }

    quicrPublicationsMap[quicrNamespace] = PublicationDelegate::create(std::move(qDelegate),
                                                                       sourceId,
                                                                       quicrNamespace,
                                                                       transport_mode,
                                                                       originUrl,
                                                                       authToken,
                                                                       std::move(payload),
                                                                       priority,
                                                                       expiry,
                                                                       logger);

    return quicrPublicationsMap[quicrNamespace]->getptr();
}

/*===========================================================================*/
// QController Delegates
/*===========================================================================*/

std::shared_ptr<QSubscriptionDelegate> QController::getSubscriptionDelegate(const SourceId& sourceId,
                                                                            const manifest::ProfileSet& profileSet)
{
    if (!qSubscriberDelegate)
    {
        LOGGER_ERROR(logger, "Subscription delegate doesn't exist for " << sourceId);
        return nullptr;
    }

    std::lock_guard<std::mutex> _(qSubsMutex);
    if (!qSubscriptionsMap.contains(sourceId))
    {
        qSubscriptionsMap[sourceId] = qSubscriberDelegate->allocateSubBySourceId(sourceId, profileSet);
    }

    return qSubscriptionsMap[sourceId];
}

std::shared_ptr<QPublicationDelegate> QController::getPublicationDelegate(const quicr::Namespace& quicrNamespace,
                                                                          const std::string& sourceID,
                                                                          const std::string& qualityProfile)
{
    if (!qPublisherDelegate)
    {
        LOGGER_ERROR(logger, "Publication delegate doesn't exist for " << quicrNamespace);
        return nullptr;
    }

    std::lock_guard<std::mutex> _(qPubsMutex);
    if (!qPublicationsMap.contains(quicrNamespace))
    {
        qPublicationsMap[quicrNamespace] = qPublisherDelegate->allocatePubByNamespace(
            quicrNamespace, sourceID, qualityProfile);
    }

    return qPublicationsMap[quicrNamespace];
}

int QController::startSubscription(std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                                   const std::string& sourceId,
                                   const quicr::Namespace& quicrNamespace,
                                   const quicr::SubscribeIntent intent,
                                   const std::string& originUrl,
                                   const bool useReliableTransport,
                                   const std::string& authToken,
                                   quicr::bytes& e2eToken)
{
    // look to see if we already have a quicr delegate
    auto sub_delegate = findQuicrSubscriptionDelegate(quicrNamespace);
    if (!sub_delegate)
    {
        sub_delegate = createQuicrSubscriptionDelegate(sourceId,
                                                       quicrNamespace,
                                                       intent,
                                                       originUrl,
                                                       useReliableTransport,
                                                       authToken,
                                                       std::move(e2eToken),
                                                       std::move(qDelegate));
    }

    if (!sub_delegate)
    {
        LOGGER_ERROR(logger, "Failed to find or create Subscription delegate for " << quicrNamespace);
        return -1;
    }

    sub_delegate->subscribe(client_session);
    return 0;
}

void QController::stopSubscription(const quicr::Namespace& quicrNamespace)
{
    if (!quicrSubscriptionsMap.contains(quicrNamespace)) return;

    auto& sub_delegate = quicrSubscriptionsMap[quicrNamespace];
    sub_delegate->unsubscribe(client_session);

    LOGGER_INFO(logger, "Unsubscribed " << quicrNamespace);
}

int QController::startPublication(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                                  std::string sourceId,
                                  const quicr::Namespace& quicrNamespace,
                                  const std::string& originUrl,
                                  const std::string& authToken,
                                  quicr::bytes&& payload,
                                  const std::vector<std::uint8_t>& priority,
                                  std::uint16_t expiry,
                                  bool reliableTransport)

{
    if (!client_session)
    {
        LOGGER_ERROR(logger, "Failed to start publication for " << quicrNamespace << ": No Quicr session established");
        return -1;
    }

    auto quicrPubDelegate = createQuicrPublicationDelegate(std::move(qDelegate),
                                                           sourceId,
                                                           quicrNamespace,
                                                           originUrl,
                                                           authToken,
                                                           std::move(payload),
                                                           priority,
                                                           expiry,
                                                           reliableTransport);
    if (!quicrPubDelegate)
    {
        LOGGER_ERROR(logger, "Failed to start publication for " << quicrNamespace << ": Delegate was null");
        return -1;
    }

    const quicr::TransportMode transport_mode = reliableTransport ? quicr::TransportMode::ReliablePerGroup
                                                                  : quicr::TransportMode::Unreliable;

     // TODO: add more intent parameters - max queue size (in time), default ttl, priority
    quicrPubDelegate->publishIntent(client_session, transport_mode);
    return 0;
}

void QController::processSubscriptions(const std::vector<manifest::MediaStream>& subscriptions)
{
    LOGGER_DEBUG(logger, "Processing subscriptions...");
    for (auto& subscription : subscriptions)
    {
        auto delegate = getSubscriptionDelegate(subscription.sourceId, subscription.profileSet);
        if (!delegate)
        {
            LOGGER_WARNING(logger, "Unable to allocate subscription delegate.");
            continue;
        }

        int update_error = delegate->update(subscription.sourceId, subscription.label, subscription.profileSet);
        if (update_error == 0)
        {
            LOGGER_INFO(logger, "Updated subscription " << subscription.sourceId);
            continue;
        }
        
        bool reliable = false;
        int prepare_error = delegate->prepare(subscription.sourceId, subscription.label, subscription.profileSet, reliable);
        if (prepare_error != 0)
        {
            LOGGER_ERROR(logger, "Error preparing subscription: " << prepare_error);
            continue;
        }

        for (const auto& profile : subscription.profileSet.profiles)
        {
            quicr::bytes e2eToken;
            startSubscription(delegate,
                              subscription.sourceId,
                              profile.quicrNamespace,
                              quicr::SubscribeIntent::sync_up,
                              "",
                              reliable,
                              "",
                              e2eToken);

                // If singleordered, and we've successfully processed 1 delegate, break.
                if (is_singleordered_subscription) break;
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
            auto delegate = getPublicationDelegate(
                profile.quicrNamespace, publication.sourceId, profile.qualityProfile);
            if (!delegate)
            {
                LOGGER_ERROR(logger, "Failed to create publication delegate: " << profile.quicrNamespace);
                continue;
            }

            // Notify client to prepare for incoming media
            bool reliable = false;
            int prepare_error = delegate->prepare(publication.sourceId, profile.qualityProfile, reliable);
            if (prepare_error != 0)
            {
                LOGGER_WARNING(logger,
                               "Preparing publication \"" << profile.quicrNamespace << "\" failed: " << prepare_error);
                continue;
            }

            quicr::bytes payload;
            startPublication(delegate,
                             publication.sourceId,
                             profile.quicrNamespace,
                             "",
                             "",
                             std::move(payload),
                             profile.priorities,
                             profile.expiry.value(),
                             reliable);

            // If singleordered, and we've successfully processed 1 delegate, break.
            if (is_singleordered_publication) break;
        }
    }

    LOGGER_INFO(logger, "Finished processing publications!");
}

void QController::updateManifest(const std::string& manifest_json)
{
    LOGGER_DEBUG(logger, "Parsing manifest...");
    const auto manifest_parsed = json::parse(manifest_json);
    const auto manifest_obj = manifest::Manifest(manifest_parsed);
    LOGGER_INFO(logger, "Finished parsing manifest!");

    updateManifest(manifest_obj);
}

void QController::updateManifest(const manifest::Manifest& manifest_obj)
{
    LOGGER_DEBUG(logger, "Importing manifest...");

    processSubscriptions(manifest_obj.subscriptions);
    processPublications(manifest_obj.publications);

    LOGGER_INFO(logger, "Finished importing manifest!");
}

}        // namespace qmedia

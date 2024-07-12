#include "qmedia/QController.hpp"
#include "qmedia/QuicrDelegates.hpp"
#include "qmedia/ManifestTypes.hpp"

#include <quicr/hex_endec.h>
#include <quicr/measurement.h>

#include <iostream>
#include <sstream>

namespace qmedia
{

QController::QController(std::shared_ptr<QSubscriberDelegate> qSubscriberDelegate,
                         std::shared_ptr<QPublisherDelegate> qPublisherDelegate,
                         const cantina::LoggerPointer& logger,
                         const bool debugging,
                         const std::optional<sframe::CipherSuite> cipher_suite) :
    logger(std::make_shared<cantina::Logger>("QCTRL", logger)),
    qSubscriberDelegate(std::move(qSubscriberDelegate)),
    qPublisherDelegate(std::move(qPublisherDelegate)),
    stop(false),
    closed(false),
    cipher_suite(cipher_suite)
{
    // If there's a parent logger, its log level will be used.
    // Otherwise, query the debugging flag.
    if (logger == nullptr && debugging)
    {
        this->logger->SetLogLevel(cantina::LogLevel::Debug);
    }

    LOGGER_DEBUG(this->logger, "QController started...");

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

int QController::connect(const std::string endpointID,
                         const std::string remoteAddress,
                         std::uint16_t remotePort,
                         quicr::RelayInfo::Protocol protocol,
                         size_t chunkSize,
                         const qtransport::TransportConfig& config)
{
    quicr::RelayInfo relayInfo = {
        .hostname = remoteAddress.c_str(),
        .port = remotePort,
        .proto = protocol,
    };

    quicr::MeasurementsConfig metrics_config{
        .metrics_namespace = quicr::Namespace("0xA11CEB0B000000000000000000000000/80"),
        .priority = 31,
        .ttl = 65535,
    };

    // SAH - add const std::string endpointId to the constructor
    client_session = std::make_unique<quicr::Client>(relayInfo, endpointID, chunkSize, config, logger, metrics_config);

    if (!client_session->connect()) return -1;

    return 0;
}

bool QController::connected() const
{
    return client_session && client_session->connected();
}

int QController::disconnect()
{
    if (closed)
    {
        return -1;
    }

    LOGGER_DEBUG(logger, "Disconnecting client session...");

    stop = true;

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
    const auto& it = quicrPublicationsMap.find(quicrNamespace);
    if (it == quicrPublicationsMap.end())
    {
        LOGGER_WARNING(logger, "Publication not found for " << quicrNamespace);
        return;
    }
    const auto& publication = it->second;
    if (publication.state != PublicationState::paused)
    {
        const auto start_time = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());

        std::vector<qtransport::MethodTraceItem> trace;
        trace.reserve(10);
        trace.push_back({"qController:publishNamedObject", start_time});

        publication.delegate->publishNamedObject(this->client_session, data, len, groupFlag, std::move(trace));
    }
}

/*
 * For Test Only
 */
void QController::publishNamedObjectTest(std::uint8_t* data, std::size_t len, bool groupFlag)
{
    const std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.empty()) return;

    const auto& publication = quicrPublicationsMap.begin()->second;
    if (publication.state != PublicationState::paused)
    {
        const auto start_time = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());

        std::vector<qtransport::MethodTraceItem> trace;
        trace.push_back({"qController:publishNamedObject", start_time});
        publication.delegate->publishNamedObject(this->client_session, data, len, groupFlag, std::move(trace));
    }
}

void QController::publishMeasurement(const quicr::Measurement& m)
{
    if (!client_session)
    {
        LOGGER_ERROR(logger, "Failed to publish measurement: No Quicr session established");
        return;
    }

    client_session->publishMeasurement(m);
}

void QController::publishMeasurement(const json& j)
{
    quicr::Measurement m = j;
    publishMeasurement(m);
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
                                             const quicr::TransportMode transportMode,
                                             const std::string& authToken,
                                             quicr::bytes&& e2eToken,
                                             std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                                             const std::optional<sframe::CipherSuite> cipherSuite)
{
    std::lock_guard<std::mutex> _(subsMutex);
    if (quicrSubscriptionsMap.contains(quicrNamespace))
    {
        LOGGER_ERROR(logger, "Quicr Subscription delegate for \"" << quicrNamespace << "\" already exists!");
        return nullptr;
    }

    quicrSubscriptionsMap[quicrNamespace] = SubscriptionDelegate::create(sourceId,
                                                                         quicrNamespace,
                                                                         intent,
                                                                         transportMode,
                                                                         originUrl,
                                                                         authToken,
                                                                         std::move(e2eToken),
                                                                         std::move(qDelegate),
                                                                         logger,
                                                                         cipherSuite);
    return quicrSubscriptionsMap[quicrNamespace];
}

std::shared_ptr<PublicationDelegate> QController::findQuicrPublicationDelegate(const quicr::Namespace& quicrNamespace)
{
    std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.contains(quicrNamespace))
    {
        return quicrPublicationsMap[quicrNamespace].delegate->getptr();
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
                                            const std::vector<std::uint16_t>& expiry,
                                            const quicr::TransportMode transportMode)
{
    std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.contains(quicrNamespace))
    {
        LOGGER_ERROR(logger, "Quicr Publication delegate for \"" << quicrNamespace << "\" already exists!");
        return nullptr;
    }

    quicrPublicationsMap[quicrNamespace] = {
        .state = PublicationState::active,
        .delegate = PublicationDelegate::create(std::move(qDelegate),
                                                sourceId,
                                                quicrNamespace,
                                                transportMode,
                                                originUrl,
                                                authToken,
                                                std::move(payload),
                                                priority,
                                                expiry,
                                                logger,
                                                cipher_suite)
    };

    return quicrPublicationsMap[quicrNamespace].delegate->getptr();
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
                                                                          const std::string& qualityProfile,
                                                                          const std::string& appTag)
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
            quicrNamespace, sourceID, qualityProfile, appTag);
    }

    return qPublicationsMap[quicrNamespace];
}

int QController::startSubscription(std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                                   const std::string& sourceId,
                                   const quicr::Namespace& quicrNamespace,
                                   const quicr::SubscribeIntent intent,
                                   const std::string& originUrl,
                                   const quicr::TransportMode transportMode,
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
                                                       transportMode,
                                                       authToken,
                                                       std::move(e2eToken),
                                                       std::move(qDelegate),
                                                       cipher_suite);
    }

    if (!sub_delegate)
    {
        LOGGER_ERROR(logger, "Failed to find or create Subscription delegate for " << quicrNamespace);
        return -1;
    }

    sub_delegate->subscribe(client_session, transportMode);
    return 0;
}

void QController::stopSubscription(const quicr::Namespace& quicrNamespace)
{
    std::lock_guard<std::mutex> _(subsMutex);
    const auto& it = quicrSubscriptionsMap.find(quicrNamespace);
    if (it == quicrSubscriptionsMap.end()) {
        LOGGER_WARNING(logger, "Subscription not found for " << quicrNamespace);
        return;
    }
    auto& sub_delegate = it->second;
    sub_delegate->unsubscribe(client_session);
    quicrSubscriptionsMap.erase(it);

    // TODO: Remove from qSubscriptionsMap if no more subscriptions
    // left in the switching set.
    LOGGER_INFO(logger, "Unsubscribed " << quicrNamespace);
}

int QController::startPublication(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                                  std::string sourceId,
                                  const quicr::Namespace& quicrNamespace,
                                  const std::string& originUrl,
                                  const std::string& authToken,
                                  quicr::bytes&& payload,
                                  const std::vector<std::uint8_t>& priority,
                                  const std::vector<std::uint16_t>& expiry,
                                  const quicr::TransportMode transportMode)

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
                                                           transportMode);
    if (!quicrPubDelegate)
    {
        LOGGER_ERROR(logger, "Failed to start publication for " << quicrNamespace << ": Delegate was null");
        return -1;
    }

     // TODO: add more intent parameters - max queue size (in time), default ttl, priority
    quicrPubDelegate->publishIntent(client_session, transportMode);
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

        auto transportMode = quicr::TransportMode::Unreliable;
        int prepare_error = delegate->prepare(subscription.sourceId, subscription.label, subscription.profileSet, transportMode);
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
                              transportMode,
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
                profile.quicrNamespace, publication.sourceId, profile.qualityProfile, profile.appTag);
            if (!delegate)
            {
                LOGGER_ERROR(logger, "Failed to create publication delegate: " << profile.quicrNamespace);
                continue;
            }

            // Notify client to prepare for incoming media
            auto transportMode = quicr::TransportMode::Unreliable;
            int prepare_error = delegate->prepare(publication.sourceId, profile.qualityProfile, transportMode);
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
                             profile.expiry,
                             transportMode);

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

std::vector<SourceId> QController::getSwitchingSets()
{
    std::lock_guard<std::mutex> _(qSubsMutex);
    std::vector<SourceId> sourceIds;
    for (const auto& switchingSet : qSubscriptionsMap) {
        sourceIds.push_back(switchingSet.first);
    }
    return sourceIds;
}

std::vector<quicr::Namespace> QController::getSubscriptions(const std::string& sourceId)
{
    std::lock_guard<std::mutex> _(subsMutex);
    std::vector<quicr::Namespace> namespaces;
    for (const auto& subscription : quicrSubscriptionsMap) {
        if (subscription.second->getSourceId() == sourceId) {
            namespaces.push_back(subscription.first);
        }
    }
    return namespaces;
}

std::vector<QController::PublicationReport> QController::getPublications()
{
    std::lock_guard<std::mutex> _(pubsMutex);
    std::vector<PublicationReport> publications;
    for (const auto& publication : quicrPublicationsMap) {
        publications.push_back({
            .state = publication.second.state,
            .quicrNamespace = publication.first,
        });
    }
    return publications;
}

void QController::setPublicationState(const quicr::Namespace& quicrNamespace, const PublicationState state)
{
    std::lock_guard<std::mutex> _(pubsMutex);
    const auto& it = quicrPublicationsMap.find(quicrNamespace);
    if (it == quicrPublicationsMap.end())
    {
        LOGGER_WARNING(logger, "Publication not found for " << quicrNamespace);
        return;
    }
    it->second.state = state;
}

void QController::setSubscriptionState(const quicr::Namespace& quicrNamespace, const quicr::TransportMode transportMode)
{
    std::lock_guard<std::mutex> _(subsMutex);
    const auto& it = quicrSubscriptionsMap.find(quicrNamespace);
    if (it == quicrSubscriptionsMap.end())
    {
        LOGGER_WARNING(logger, "Subscription not found for " << quicrNamespace);
        return;
    }
    it->second->subscribe(client_session, transportMode);
}

quicr::SubscriptionState QController::getSubscriptionState(const quicr::Namespace& quicrNamespace)
{
    return client_session->getSubscriptionState(quicrNamespace);
}

}        // namespace qmedia

#include <qmedia/QController.hpp>
#include <qmedia/QuicrDelegates.hpp>

#include <quicr/hex_endec.h>
#include <transport/transport.h>

#include <iostream>
#include <sstream>

namespace
{
constexpr std::string_view SingleOrderedStr = "singleordered";
constexpr std::string_view SimulcastStr = "singleordered";
}        // namespace

namespace qmedia
{

QController::QController(const std::shared_ptr<QSubscriberDelegate>& qSubscriberDelegate,
                         const std::shared_ptr<QPublisherDelegate>& qPublisherDelegate,
                         const cantina::LoggerPointer& logger) :
    logger(std::make_shared<cantina::Logger>("QCTRL", logger)),
    qSubscriberDelegate(qSubscriberDelegate),
    qPublisherDelegate(qPublisherDelegate),
    stop(false),
    closed(false)
{
    LOGGER_INFO(logger, "QController started...");

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
    close();
}

int QController::connect(const std::string remoteAddress, std::uint16_t remotePort, quicr::RelayInfo::Protocol protocol)
{
    quicr::RelayInfo relayInfo = {
        .hostname = remoteAddress.c_str(),
        .port = remotePort,
        .proto = protocol,
    };

    qtransport::TransportConfig tcfg{
        .tls_cert_filename = NULL,
        .tls_key_filename = NULL,
        .time_queue_init_queue_size = 200,
    };

    quicrClient = std::make_unique<quicr::QuicRClient>(relayInfo, std::move(tcfg), logger);

    if (!quicrClient->connect()) return -1;

    // check to see if there is a timer thread...
    keepaliveThread = std::thread(&QController::periodicResubscribe, this, 5);
    return 0;
}

int QController::disconnect()
{
    return 0;
}

void QController::close()
{
    if (closed)
    {
        return;
    }

    // shutdown everything...
    stop = true;
    if (keepaliveThread.joinable())
    {
        keepaliveThread.join();
    }

    if (quicrClient) quicrClient->disconnect();

    LOGGER_INFO(logger, "Closed all connections!");
    closed = true;
}

void QController::periodicResubscribe(const unsigned int seconds)
{
    std::chrono::system_clock::time_point timeout = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
    while (!stop)
    {
        std::chrono::duration<int, std::milli> timespan(100);        // sleep duration in mills
        std::this_thread::sleep_for(timespan);
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

        if (now >= timeout && !stop)
        {
            LOGGER_INFO(logger, "re-subscribe");
            const std::lock_guard<std::mutex> _(subsMutex);
            for (auto const& [key, quicrSubDelegate] : quicrSubscriptionsMap)
            {
                quicrSubDelegate->subscribe(quicrSubDelegate, quicrClient);
            }
            timeout = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
        }
    }
    std::cerr << "QController::periodicResubscribe - ended" << std::endl;
}

void QController::removeSubscriptions()
{
    LOGGER_INFO(logger, "QController - remove subscriptions");
    const std::lock_guard<std::mutex> _(subsMutex);
    for (auto const& [key, quicrSubDelegate] : quicrSubscriptionsMap)
    {
        quicrSubDelegate->unsubscribe(quicrSubDelegate, quicrClient);
    }
}

void QController::publishNamedObject(const quicr::Namespace& quicrNamespace,
                                     std::uint8_t* data,
                                     std::size_t len,
                                     bool groupFlag)
{
    const std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.count(quicrNamespace))
    {
        auto publicationDelegate = quicrPublicationsMap.at(quicrNamespace);
        if (publicationDelegate)
        {
            publicationDelegate->publishNamedObject(this->quicrClient, data, len, groupFlag);
        }
    }
}

/*
 * For Test Only
 */
void QController::publishNamedObjectTest(std::uint8_t* data, std::size_t len, bool groupFlag)
{
    const std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.size() > 0)
    {
        auto publicationDelegate = quicrPublicationsMap.begin()->second;
        publicationDelegate->publishNamedObject(this->quicrClient, data, len, groupFlag);
    }
}

/*===========================================================================*/
// Quicr Delegates
/*===========================================================================*/

std::shared_ptr<QuicrTransportSubDelegate>
QController::findQuicrSubscriptionDelegate(const quicr::Namespace& quicrNamespace)
{
    std::lock_guard<std::mutex> _(subsMutex);
    if (quicrSubscriptionsMap.contains(quicrNamespace))
    {
        return quicrSubscriptionsMap[quicrNamespace];
    }
    return nullptr;
}

std::shared_ptr<QuicrTransportSubDelegate>
QController::createQuicrSubscriptionDelegate(const std::string sourceId,
                                             const quicr::Namespace& quicrNamespace,
                                             const quicr::SubscribeIntent intent,
                                             const std::string originUrl,
                                             const bool useReliableTransport,
                                             const std::string authToken,
                                             quicr::bytes e2eToken,
                                             std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate)
{
    std::lock_guard<std::mutex> _(subsMutex);
    if (quicrSubscriptionsMap.count(quicrNamespace))
    {
        LOGGER_ERROR(logger, "Quicr Subscription delegate for \"" << quicrNamespace << "\" already exists!");
        return nullptr;
    }

    quicrSubscriptionsMap[quicrNamespace] = std::make_shared<qmedia::QuicrTransportSubDelegate>(
        sourceId, quicrNamespace, intent, originUrl, useReliableTransport, authToken, e2eToken, qDelegate, logger);
    return quicrSubscriptionsMap[quicrNamespace];
}

std::shared_ptr<QuicrTransportPubDelegate>
QController::findQuicrPublicationDelegate(const quicr::Namespace& quicrNamespace)
{
    std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.count(quicrNamespace))
    {
        return quicrPublicationsMap[quicrNamespace];
    }
    return nullptr;
}

std::shared_ptr<QuicrTransportPubDelegate>
QController::createQuicrPublicationDelegate(const std::string sourceId,
                                            const quicr::Namespace& quicrNamespace,
                                            const std::string& originUrl,
                                            const std::string& authToken,
                                            quicr::bytes&& payload,
                                            const std::vector<std::uint8_t>& priority,
                                            std::uint16_t expiry,
                                            bool reliableTransport,
                                            std::shared_ptr<qmedia::QPublicationDelegate> qDelegate)
{
    std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.count(quicrNamespace))
    {
        LOGGER_ERROR(logger, "Quicr Publication delegate for \"" << quicrNamespace << "\" already exists!");
        return nullptr;
    }

    quicrPublicationsMap[quicrNamespace] = std::make_shared<qmedia::QuicrTransportPubDelegate>(sourceId,
                                                                                               quicrNamespace,
                                                                                               originUrl,
                                                                                               authToken,
                                                                                               std::move(payload),
                                                                                               priority,
                                                                                               expiry,
                                                                                               reliableTransport,
                                                                                               qDelegate,
                                                                                               logger);

    return quicrPublicationsMap[quicrNamespace];
}

/*===========================================================================*/
// QController Delegates
/*===========================================================================*/

std::shared_ptr<QSubscriptionDelegate> QController::getSubscriptionDelegate(const quicr::Namespace& quicrNamespace,
                                                                            const std::string& qualityProfile)
{
    if (!qSubscriberDelegate)
    {
        LOGGER_ERROR(logger, "Subscription delegate doesn't exist");
        return nullptr;
    }

    std::lock_guard<std::mutex> _(qSubsMutex);
    if (!qSubscriptionsMap.count(quicrNamespace))
    {
        qSubscriptionsMap[quicrNamespace] = qSubscriberDelegate->allocateSubByNamespace(quicrNamespace, qualityProfile);
    }

    return qSubscriptionsMap[quicrNamespace];
}

std::shared_ptr<QPublicationDelegate> QController::getPublicationDelegate(const quicr::Namespace& quicrNamespace,
                                                                          const std::string& sourceID,
                                                                          const std::string& qualityProfile)
{
    if (!qPublisherDelegate)
    {
        LOGGER_ERROR(logger, "Publication delegate doesn't exist");
        return nullptr;
    }

    std::lock_guard<std::mutex> _(qPubsMutex);
    if (!qPublicationsMap.count(quicrNamespace))
    {
        qPublicationsMap[quicrNamespace] = qPublisherDelegate->allocatePubByNamespace(
            quicrNamespace, sourceID, qualityProfile);
    }

    return qPublicationsMap[quicrNamespace];
}

int QController::startSubscription(std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                                   std::string sourceId,
                                   const quicr::Namespace& quicrNamespace,
                                   const quicr::SubscribeIntent intent,
                                   const std::string originUrl,
                                   const bool useReliableTransport,
                                   const std::string authToken,
                                   quicr::bytes e2eToken)
{
    // look to see if we already have a quicr delegate
    auto quicrSubDelegate = findQuicrSubscriptionDelegate(quicrNamespace);
    if (quicrSubDelegate == nullptr)
    {
        quicrSubDelegate = createQuicrSubscriptionDelegate(
            sourceId, quicrNamespace, intent, originUrl, useReliableTransport, authToken, e2eToken, qDelegate);
    }

    if (!quicrSubDelegate)
    {
        LOGGER_ERROR(logger, "Starting subscription: Failed to find or create delegate");
        return -1;
    }

    quicrSubDelegate->subscribe(quicrSubDelegate, quicrClient);
    return 0;
}

void QController::stopSubscription(const quicr::Namespace& /* quicrNamespace */)
{
    // TODO: What do we want to do here?
    // TODO(trigaux): Prompt Decimus to remove/stop rendering subscription possibly? Or maybe remove it?
    LOGGER_ERROR(logger, __PRETTY_FUNCTION__ << " is not implemented.");
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
    if (!quicrClient)
    {
        LOGGER_ERROR(logger, "Starting publication: No Quicr session established");
        return -1;
    }

    auto quicrPubDelegate = createQuicrPublicationDelegate(sourceId,
                                                           quicrNamespace,
                                                           originUrl,
                                                           authToken,
                                                           std::move(payload),
                                                           priority,
                                                           expiry,
                                                           reliableTransport,
                                                           qDelegate);
    if (!quicrPubDelegate)
    {
        LOGGER_ERROR(logger, "Starting publication: Delegate was null");
        return -1;
    }

    // TODO: add more intent parameters - max queue size (in time), default ttl, priority
    quicrPubDelegate->publishIntent(quicrPubDelegate, quicrClient, reliableTransport);
    return 0;
}

int QController::processURLTemplates(json& urlTemplates)
{
    LOGGER_DEBUG(logger, "Processing URL templates...");
    for (auto& urlTemplate : urlTemplates)
    {
        std::string temp = urlTemplate;
        encoder.AddTemplate(temp, true);
    }
    LOGGER_INFO(logger, "Finished processing templates!");
    return 0;
}

int QController::processSubscriptions(json& subscriptions)
{
    LOGGER_DEBUG(logger, "Processing subscriptions...");
    for (auto& subscription : subscriptions)
    {
        for (auto& profile : subscription["profileSet"]["profiles"])
        {
            quicr::Namespace quicrNamespace = encoder.EncodeUrl(profile["quicrNamespaceUrl"]);

            // look up subscription delegate or allocate new one
            auto qSubscriptionDelegate = getSubscriptionDelegate(quicrNamespace, profile["qualityProfile"]);
            if (!qSubscriptionDelegate)
            {
                LOGGER_WARNING(logger, "Unable to allocate subscription delegate.");
                continue;
            }

            int rc = qSubscriptionDelegate->update(
                subscription["sourceId"], subscription["label"], profile["qualityProfile"]);

            if (rc != 0)
            {
                // notify client to prepare for incoming media
                bool reliable = false;
                int prepareError = qSubscriptionDelegate->prepare(
                    subscription["sourceId"], subscription["label"], profile["qualityProfile"], reliable);

                if (prepareError != 0)
                {
                    LOGGER_ERROR(logger, "Error preparing subscription: " << prepareError);
                    continue;
                }

                quicr::bytes e2eToken;
                startSubscription(qSubscriptionDelegate,
                                  subscription["sourceId"],
                                  quicrNamespace,
                                  quicr::SubscribeIntent::sync_up,
                                  "",
                                  reliable,
                                  "",
                                  e2eToken);
            }

            // If singleordered, and we've successfully processed 1 delegate, break.
            if (subscription["profileSet"]["type"] == SingleOrderedStr) break;
        }
    }

    LOGGER_INFO(logger, "Finished processing subscriptions!");
    return 0;
}

int QController::processPublications(json& publications)
{
    LOGGER_DEBUG(logger, "Processing publications...");
    for (auto& publication : publications)
    {
        for (auto& profile : publication["profileSet"]["profiles"])
        {
            quicr::Namespace quicrNamespace = encoder.EncodeUrl(profile["quicrNamespaceUrl"]);

            auto qPublicationDelegate = getPublicationDelegate(
                quicrNamespace, publication["sourceId"], profile["qualityProfile"]);
            if (!qPublicationDelegate)
            {
                LOGGER_ERROR(logger, "Failed to create publication delegate: " << quicrNamespace);
                continue;
            }

            // Notify client to prepare for incoming media
            bool reliable = false;
            int prepareError = qPublicationDelegate->prepare(publication["sourceId"], profile["qualityProfile"], reliable);
            if (prepareError != 0)
            {
                LOGGER_WARNING(logger, "Preparing publication \"" << quicrNamespace << "\" failed: " << prepareError);
                continue;
            }

            quicr::bytes payload;
            startPublication(qPublicationDelegate,
                             publication["sourceId"],
                             quicrNamespace,
                             "",
                             "",
                             std::move(payload),
                             profile["priorities"],
                             profile["expiry"],
                             reliable);

            // If singleordered, and we've successfully processed 1 delegate, break.
            if (publication["profileSet"]["type"] == SingleOrderedStr) break;
        }
    }

    LOGGER_INFO(logger, "Finished processing publications!");
    return 0;
}

int QController::updateManifest(const std::string manifest)
{
    auto manifest_object = json::parse(manifest);

    LOGGER_DEBUG(logger, "Parsing manifest...");

    processURLTemplates(manifest_object["urlTemplates"]);
    processSubscriptions(manifest_object["subscriptions"]);
    processPublications(manifest_object["publications"]);

    LOGGER_INFO(logger, "Finished parsing manifest!");

    return 0;
}

}        // namespace qmedia

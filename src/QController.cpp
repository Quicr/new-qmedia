#include <qmedia/QController.hpp>
#include <quicr/hex_endec.h>
#include <qmedia/QuicrDelegates.hpp>
#include <iostream>

namespace qmedia
{

QController::QController(std::shared_ptr<QSubscriberDelegate> qSubscriberDelegate,
                         std::shared_ptr<QPublisherDelegate> qPublisherDelegate) :
    qSubscriberDelegate(qSubscriberDelegate), qPublisherDelegate(qPublisherDelegate)
{
    logger.log(qtransport::LogLevel::info, "QController started...");

    // quicr://webex.cisco.com/conference/1/mediaType/192/endpoint/2
    //   org, app,   conf, media, endpoint,     group, object
    //   24,   8,      24,     8,       16,        32,     16
    // 000001  01   000001     c0      0002  / 00000000,  0000
    // pen - private enterprise number (24 bits)
    // sub_pen - sub-privete enterprise number (8 bits)

    // SAH - fixme - rename "mediaType" -> "mediatype"
    // SAH - fixme - rename "conference" -> "conferences"
    encoder.AddTemplate(std::string("quicr://webex.cisco.com"
                                    "<pen=1><sub_pen=1>/conference/<int24>/mediaType/"
                                    "<int8>/endpoint/<int16>"));
}

QController::~QController()
{
    // shutdown everything...
}

int QController::connect(const std::string remoteAddress, std::uint16_t remotePort, quicr::RelayInfo::Protocol protocol)
{
    quicr::RelayInfo relayInfo = {.hostname = remoteAddress.c_str(), .port = remotePort, .proto = protocol};

    qtransport::TransportConfig tcfg{.tls_cert_filename = NULL, .tls_key_filename = NULL, .data_queue_size = 200};

    // Bridge to external logging.
    quicrClient = std::make_unique<quicr::QuicRClient>(relayInfo, std::move(tcfg), logger);
    if (quicrClient == nullptr)
    {
        return -1;
    }

    // check to see if there is a timer thread...
    keepaliveThread = std::thread(&QController::periodicResubscribe, this, 5);
    return 0;
}

int QController::disconnect()
{
}

void QController::close()
{
    stop = true;
    keepaliveThread.join();        // waif for thread to go away...
    {
        const std::lock_guard<std::mutex> _(subsMutex);
        quicrSubscriptionsMap.clear();
        qSubscriptionsMap.clear();
    }

    {
        const std::lock_guard<std::mutex> _(pubsMutex);
        quicrPublicationsMap.clear();
        qPublicationsMap.clear();
    }
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
             logger.log(qtransport::LogLevel::info, "re-subscribe");
            const std::lock_guard<std::mutex> _(subsMutex);
            for (auto const& [key, quicrSubDelegate] : quicrSubscriptionsMap)
            {

                quicrSubDelegate->subscribe(quicrSubDelegate, quicrClient);
            }
            timeout = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
        }
    }
}

void QController::publishNamedObject(const quicr::Namespace& quicrNamespace, std::uint8_t* data, std::size_t len)
{
    if (qPublicationsMap.count(quicrNamespace))
    {
        auto publicationDelegate = qPublicationsMap.at(quicrNamespace);
        if (publicationDelegate)
        {
            publicationDelegate->publishNamedObject(this->quicrClient, data, len);
        }
    }
}

/*
 * For Test Only
 */
void QController::publishNamedObjectTest(std::uint8_t* data, std::size_t len)
{
    if (qPublicationsMap.size() > 0)
    {
        // auto quicrNamespace = qPublicationsMap.begin()->first;
        auto publicationDelegate = qPublicationsMap.begin()->second;
        publicationDelegate->publishNamedObject(this->quicrClient, data, len);
    }
}

quicr::Namespace QController::quicrNamespaceUrlParse(const std::string& quicrNamespaceUrl)
{
    auto encodedTemplate = encoder.GetTemplate(1).at(1);
    uint8_t bits = 24 + 8;
    for (auto bitCount : encodedTemplate.bits)
    {
        bits += bitCount;
    }
    quicr::Name encoded = encoder.EncodeUrl(quicrNamespaceUrl);
    quicr::Name DELETE_ME = ((encoded << 32) >> 32) | 0xA11CEE00000000000000000000000000_name;

    quicr::Namespace quicrNamespace{DELETE_ME, bits};
    return quicrNamespace;
}

/////////////
// Quic Delegates
////////////
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
QController::createQuicrSubsciptionDelegate(const std::string sourceId,
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
        logger.log(qtransport::LogLevel::error,
                   "Error: creating QuicrTransportSubDelegatefor namespace already exists!");
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
                                            std::shared_ptr<qmedia::QPublicationDelegate> qDelegate)
{
    std::lock_guard<std::mutex> _(pubsMutex);
    if (quicrPublicationsMap.count(quicrNamespace))
    {
        logger.log(qtransport::LogLevel::error,
                   "Error: creating QuicrTransportSubDelegate for namespace - already exists!");
        return nullptr;
    }
    
    quicrPublicationsMap[quicrNamespace] = std::make_shared<qmedia::QuicrTransportPubDelegate>(
        sourceId, quicrNamespace, originUrl, authToken, std::move(payload), qDelegate, logger);
    return quicrPublicationsMap[quicrNamespace];
}

/////////////
// QController Delegates
////////////
std::shared_ptr<QSubscriptionDelegate> QController::getSubscriptionDelegate(const quicr::Namespace& quicrNamespace)
{
    if (qSubscriberDelegate)
    {
        // look up
        std::lock_guard<std::mutex> _(subsMutex);
        // found - return
        if (!qSubscriptionsMap.count(quicrNamespace))
        {
            qSubscriptionsMap[quicrNamespace] = qSubscriberDelegate->allocateSubByNamespace(quicrNamespace);
        }

        return qSubscriptionsMap[quicrNamespace];
    }

    logger.log(qtransport::LogLevel::error, "Error: getting subscription delegate.");
    return nullptr;
}

std::shared_ptr<QPublicationDelegate> QController::getPublicationDelegate(const quicr::Namespace& quicrNamespace)
{
    if (qPublisherDelegate)
    {
        // look up
        std::lock_guard<std::mutex> _(pubsMutex);
        // found - return
        if (!qPublicationsMap.count(quicrNamespace))
        {
            qPublicationsMap[quicrNamespace] = qPublisherDelegate->allocatePubByNamespace(quicrNamespace);
        }
        return qPublicationsMap[quicrNamespace];
    }
    logger.log(qtransport::LogLevel::error, "Error: getting publication delegate.");
    return nullptr;
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
    std::shared_ptr<QuicrTransportSubDelegate> quicrSubDelegate = findQuicrSubscriptionDelegate(quicrNamespace);
    if (quicrSubDelegate == nullptr)
    {
        // didn't find one - allocate one
        quicrSubDelegate = createQuicrSubsciptionDelegate(
            sourceId, quicrNamespace, intent, originUrl, useReliableTransport, authToken, e2eToken, qDelegate);
    }

    // use this delegate for subscription
    if (quicrSubDelegate)
    {
        quicrSubDelegate->subscribe(quicrSubDelegate, quicrClient);
    }
    return 0;
}

void QController::stopSubscription(const quicr::Namespace& /* quicrNamespace */)
{
    // what do we want to do here?
    logger.log(qtransport::LogLevel::error, "Error:stopSubscription - not implmented.");
}

int QController::startPublication(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                                  std::string sourceId,
                                  const quicr::Namespace& quicrNamespace,
                                  const std::string& originUrl,
                                  const std::string& authToken,
                                  quicr::bytes&& payload /**/)
{
    auto quicrPubDelegate = createQuicrPublicationDelegate(sourceId, quicrNamespace, originUrl, authToken, std::move(payload),  qDelegate);
    if (quicrPubDelegate != nullptr)
    {
        if (quicrClient)
            // add more intent parameters - max queue size (in time), default ttl, priority
            quicrPubDelegate->publishIntent(quicrPubDelegate, quicrClient);
        return 0;
    }
    logger.log(qtransport::LogLevel::error, "Error: starting pulibcation.");
    return -1;
}

int QController::processSubscriptions(json& subscriptions)
{
    logger.log(qtransport::LogLevel::info, "processSubscriptions");
    for (auto& subscription : subscriptions)
    {
        for (auto& profile : subscription["profileSet"]["profiles"])
        {
            // get namespace from manitfest
            quicr::Namespace quicrNamespace = quicrNamespaceUrlParse(profile["quicrNamespaceUrl"]);

            // look up subscription delegate or allocate new one
            auto qSubscriptionDelegate = getSubscriptionDelegate(quicrNamespace);

            if (qSubscriptionDelegate)
            {
                int rc = qSubscriptionDelegate->update(
                    subscription["sourceId"], subscription["label"], profile["qualityProfile"]);

                if (rc == 0)
                {
                    // delegate was able to udpate the codec... nothing else required.
                }
                else if (rc == 1)
                {
                    // notify client to prepare for incoming media
                    if (qSubscriptionDelegate->prepare(
                            subscription["sourceId"], subscription["label"], profile["qualityProfile"]) == 0)
                    {
                        quicr::bytes e2eToken;
                        startSubscription(qSubscriptionDelegate,
                                          subscription["sourceId"],
                                          quicrNamespace,
                                          quicr::SubscribeIntent::sync_up,
                                          "",
                                          false,
                                          "",
                                          e2eToken);
                    }
                    else
                    {
                        // client wasn't able to prepare
                        // don't subscribe
                        logger.log(qtransport::LogLevel::error, "Error preparing subscription.");
                    }
                }

                // If singleorderd and we have a subscription prepared
                if (qSubscriptionsMap.size() > 0 && subscription["profileSet"]["type"] == "singleordered")
                {
                    break;
                }
            }
            else
            {
                // LOG - unable to allocate a subscrpition delegate
                logger.log(qtransport::LogLevel::warn, "Unable to allocate subscription delegate.");
            }
        }
    }
    return 0;
}

int QController::processPublications(json& publications)
{
    logger.log(qtransport::LogLevel::info, "processPublications");
    for (auto& publication : publications)
    {
        for (auto& profile : publication["profileSet"]["profiles"])
        {
            // get namespace from manitfest
            quicr::Namespace quicrNamespace = quicrNamespaceUrlParse(profile["quicrNamespaceUrl"]);

            // get a new subscription from the subscriber
            auto qPublicationDelegate = getPublicationDelegate(quicrNamespace);
            if (qPublicationDelegate)
            {
                // notify client to prepare for incoming media
                if (qPublicationDelegate->prepare(publication["sourceId"], profile["qualityProfile"]) == 0)
                {
                    quicr::bytes payload;
                    startPublication(
                        qPublicationDelegate, publication["sourceId"], quicrNamespace, "", "", std::move(payload));
                }
                else
                {
                    // client wasn't able to prepare
                    // don't subscribe
                    logger.log(qtransport::LogLevel::warn, "Unable to prepare publication.");
                }
            }
            else
            {
                // LOG - unable to allocate a subscrpition delegate
                logger.log(qtransport::LogLevel::error, "Couldn't get publication delegate!");
            }
        }
    }
    return 0;
}

int QController::updateManifest(const std::string manifest)
{
    // parse manifest
    auto manifest_object = json::parse(manifest);

    logger.log(qtransport::LogLevel::info, "updateManifest");

    processSubscriptions(manifest_object["Subscriptions"]);
    processPublications(manifest_object["Publications"]);

    return 0;
}

}        // namespace qmedia

#pragma once

#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>
/// #include <qmedia/QDelegates.hpp>
#include <transport/logger.h>
#include "UrlEncoder.h"
#include <nlohmann/json.hpp>
#include "QuicrDelegates.hpp"
using json = nlohmann::json;
#include <mutex>
#include <thread>
#include "basicLogger.h"

namespace qmedia
{

class QController
{
public:
    QController(std::shared_ptr<QSubscriberDelegate> qSusbscriberDelefate,
                std::shared_ptr<QPublisherDelegate> QPublisherDelegate);

    ~QController();
    int connect(const std::string remoteAddress, std::uint16_t remotePort, quicr::RelayInfo::Protocol protocol);
    int disconnect();

    void close();

    int updateManifest(const std::string manifest);

    void publishNamedObject(const quicr::Namespace& quicrNamespace, std::uint8_t* data, std::size_t len);
    void publishNamedObjectTest(std::uint8_t* data, std::size_t len);

private:
    void periodicResubscribe(const unsigned int seconds);

    quicr::Namespace quicrNamespaceUrlParse(const std::string& quicrNamespaceUrl);

    std::shared_ptr<QuicrTransportSubDelegate> findQuicrSubscriptionDelegate(const quicr::Namespace& quicrNamespace);

    std::shared_ptr<QuicrTransportSubDelegate>
    createQuicrSubsciptionDelegate(const std::string,
                                   const quicr::Namespace&,
                                   const quicr::SubscribeIntent intent,
                                   const std::string originUrl,
                                   const bool useReliableTransport,
                                   const std::string authToken,
                                   quicr::bytes e2eToken,
                                   std::shared_ptr<qmedia::QSubscriptionDelegate>);

    std::shared_ptr<QuicrTransportPubDelegate> findQuicrPublicationDelegate(const quicr::Namespace& quicrNamespace);

    std::shared_ptr<QuicrTransportPubDelegate>
    createQuicrPublicationDelegate(const std::string,
                                   const quicr::Namespace&,
                                   const std::string& originUrl,
                                   const std::string& authToken,
                                   quicr::bytes&& payload,
                                   std::shared_ptr<qmedia::QPublicationDelegate>);

    std::shared_ptr<QSubscriptionDelegate> getSubscriptionDelegate(const quicr::Namespace& quicrNamespace);
    std::shared_ptr<QPublicationDelegate> getPublicationDelegate(const quicr::Namespace& quicrNamespace);

    int startSubscription(std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                          const std::string sourceId,
                          const quicr::Namespace& quicrNamespace,
                          const quicr::SubscribeIntent intent,
                          const std::string originUrl,
                          const bool useReliableTransport,
                          const std::string authToken,
                          quicr::bytes e2eToken);

    void stopSubscription(const quicr::Namespace& quicrNamespace);

    int startPublication(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                         const std::string sourceId,
                         const quicr::Namespace& quicrNamespace,
                         const std::string& origin_url,
                         const std::string& auth_token,
                         quicr::bytes&& payload);

    void stopPublication(const quicr::Namespace& quicrNamespace);

    int processSubscriptions(json&);
    int processPublications(json&);

private:
    std::mutex subsMutex;
    std::mutex pubsMutex;

    qmedia::basicLogger logger;

    UrlEncoder encoder;

    std::shared_ptr<quicr::QuicRClient> quicrClient;

    std::shared_ptr<QSubscriberDelegate> qSubscriberDelegate;
    std::shared_ptr<QPublisherDelegate> qPublisherDelegate;

    std::map<quicr::Namespace, std::shared_ptr<QSubscriptionDelegate>> qSubscriptionsMap;
    std::map<quicr::Namespace, std::shared_ptr<QPublicationDelegate>> qPublicationsMap;

    std::map<quicr::Namespace, std::shared_ptr<QuicrTransportSubDelegate>> quicrSubscriptionsMap;
    std::map<quicr::Namespace, std::shared_ptr<QuicrTransportPubDelegate>> quicrPublicationsMap;

    std::thread keepaliveThread;
    bool stop;
};

}        // namespace qmedia

#pragma once

#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>
#include <qmedia/QDelegates.hpp>
#include <transport/logger.h>
#include "UrlEncoder.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include <mutex>
#include "basicLogger.h"

namespace qmedia
{

class QController
{
public:
    QController(std::shared_ptr<QSubscriberDelegate> qSusbscriberDelefate,
                std::shared_ptr<QPublisherDelegate> QPublisherDelegate);

    ~QController();
    void closeAll();
    int connect(const std::string remoteAddress, std::uint16_t remotePort, quicr::RelayInfo::Protocol protocol);
    int disconnect();

    int updateManifest(const std::string manifest);

    void publishNamedObject(const quicr::Namespace& quicrNamespace, std::uint8_t *data, std::size_t len);
    void publishNamedObjectTest(std::uint8_t *data, std::size_t len);

private:
    quicr::Namespace quicrNamespaceUrlParse(const std::string& quicrNamespaceUrl);

    std::shared_ptr<quicr::SubscriberDelegate> findQuicrSubscriptionDelegate(const quicr::Namespace& quicrNamespace);

    std::shared_ptr<quicr::SubscriberDelegate>
    createQuicrSubsciptionDelegate(const std::string,
                                   const quicr::Namespace&,
                                   std::shared_ptr<qmedia::QSubscriptionDelegate>);

    std::shared_ptr<quicr::PublisherDelegate> findQuicrPublicationDelegate(const quicr::Namespace& quicrNamespace);

    std::shared_ptr<quicr::PublisherDelegate>
    createQuicrPublicationDelegate(const std::string,
                                   const quicr::Namespace&,
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

    std::map<quicr::Namespace, std::shared_ptr<quicr::SubscriberDelegate>> quicrSubscriptionsMap;
    std::map<quicr::Namespace, std::shared_ptr<quicr::PublisherDelegate>> quicrPublicationsMap;
};

}        // namespace qmedia

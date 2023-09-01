#pragma once

#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>
#include <cantina/logger.h>
#include <qmedia/QDelegates.hpp>
#include <string>
#include "QSFrameContext.hpp"

namespace qmedia
{

class QuicrTransportSubDelegate : public quicr::SubscriberDelegate
{
public:
    QuicrTransportSubDelegate(const std::string sourceId,
                              const quicr::Namespace& quicrNamespace,
                              const quicr::SubscribeIntent intent,
                              const std::string originUrl,
                              const bool useReliableTransport,
                              const std::string authToken,
                              quicr::bytes e2eToken,
                              std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                              const cantina::LoggerPointer& logger);

    ~QuicrTransportSubDelegate();

    virtual void onSubscribeResponse(const quicr::Namespace& quicrNamespace, const quicr::SubscribeResult& result);

    virtual void onSubscriptionEnded(const quicr::Namespace& quicrNamespace,
                                     const quicr::SubscribeResult::SubscribeStatus& result);

    virtual void onSubscribedObject(const quicr::Name& quicrName,
                                    uint8_t priority,
                                    uint16_t expiry_age_ms,
                                    bool use_reliable_transport,
                                    quicr::bytes&& data);

    virtual void
    onSubscribedObjectFragment(const quicr::Name&, uint8_t, uint16_t, bool, const uint64_t&, bool, quicr::bytes&&)
    {
    }

    bool isActive() { return canReceiveSubs; }

    void subscribe(std::shared_ptr<QuicrTransportSubDelegate> self, std::shared_ptr<quicr::QuicRClient> quicrClient);
    void unsubscribe(std::shared_ptr<QuicrTransportSubDelegate> self, std::shared_ptr<quicr::QuicRClient> quicrClient);

private:
    bool canReceiveSubs;
    std::string sourceId;
    quicr::Namespace quicrNamespace;
    quicr::SubscribeIntent intent;
    std::string originUrl;
    bool useReliableTransport;
    std::string authToken;
    quicr::bytes e2eToken;
    std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate;
    const cantina::LoggerPointer logger;

    std::uint64_t groupCount;
    std::uint64_t objectCount;
    std::uint64_t groupGapCount;
    std::uint64_t objectGapCount;

    std::uint32_t currentGroupId;
    std::uint16_t currentObjectId;

    QSFrameContext sframe_context;
};

class QuicrTransportPubDelegate : public quicr::PublisherDelegate
{
public:
    QuicrTransportPubDelegate(std::string sourceId,
                              quicr::Namespace quicrNamespace,
                              const std::string& originUrl,
                              const std::string& authToken,
                              quicr::bytes&& payload,
                              const std::vector<std::uint8_t>& priority,
                              std::uint16_t expiry,
                              bool reliableTransport,
                              std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                              const cantina::LoggerPointer& logger);

    ~QuicrTransportPubDelegate();

    virtual void onPublishIntentResponse(const quicr::Namespace& quicr_namespace,
                                         const quicr::PublishIntentResult& result);

    void publishIntent(std::shared_ptr<QuicrTransportPubDelegate> self,
                       std::shared_ptr<quicr::QuicRClient> quicrClient,
                       bool reliableTransport=false);

    void publishIntentEnd(std::shared_ptr<QuicrTransportPubDelegate> self,
                          std::shared_ptr<quicr::QuicRClient> quicrClient);

    void publishNamedObject(std::shared_ptr<quicr::QuicRClient> quicrClient,
                            std::uint8_t* data,
                            std::size_t len,
                            bool groupFlag);

private:
    // bool canPublish;
    std::string sourceId;
    quicr::Namespace quicrNamespace;
    const std::string& originUrl;
    const std::string& authToken;
    quicr::bytes&& payload;
    std::uint32_t groupId;
    std::uint16_t objectId;
    std::vector<std::uint8_t> priority;
    std::uint16_t expiry;
    bool reliableTransport;
    std::shared_ptr<qmedia::QPublicationDelegate> qDelegate;
    const cantina::LoggerPointer logger;

    std::uint64_t groupCount;
    std::uint64_t objectCount;

    QSFrameContext sframe_context;
};
}        // namespace qmedia

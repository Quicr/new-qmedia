#pragma once

#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>
#include <transport/logger.h>
#include <qmedia/QDelegates.hpp>


namespace qmedia
{

class QuicrTransportSubDelegate : public quicr::SubscriberDelegate
{
public:
    QuicrTransportSubDelegate(const std::string sourceId,
                              const quicr::Namespace& quicrNamespace,
                              std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                              qtransport::LogHandler& logger);

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

private:
    bool canReceiveSubs;
    std::string sourceId;
    quicr::Namespace quicrNamespace;
    std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate;
    qtransport::LogHandler logger;
};

class QuicrTransportPubDelegate : public quicr::PublisherDelegate
{
public:
    QuicrTransportPubDelegate(std::string sourceId,
                              quicr::Namespace quicrNamespace,
                              std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                              qtransport::LogHandler& logger);
    virtual void onPublishIntentResponse(const quicr::Namespace& quicr_namespace,
                                         const quicr::PublishIntentResult& result);

private:
    bool canPublish;
    std::string sourceId;    
    quicr::Namespace quicrNamespace;
    std::shared_ptr<qmedia::QPublicationDelegate> qDelegate;
    qtransport::LogHandler logger;
};
}        // namespace qmedia
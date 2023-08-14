#ifndef QDelegates_h
#define QDelegates_h

#include <quicr/namespace.h>
#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>
#include <string>
#include <iostream>

namespace qmedia
{
class QSubscriptionDelegate
{
public:
    QSubscriptionDelegate() = default;

    ~QSubscriptionDelegate() {
        std::cerr << "~QSubscriptionDelegate" << std::endl;
    }
public:
    virtual int prepare(const std::string& sourceId, const std::string& label, const std::string& qualityProfile) = 0;
    virtual int update(const std::string& sourceId, const std::string& label, const std::string& qualityProfile) = 0;
    virtual int subscribedObject(quicr::bytes&& data, std::uint32_t groupId, std::uint16_t objectId) = 0;
};
class QPublicationDelegate
{
public:
    QPublicationDelegate(const quicr::Namespace& quicrNamespace) :
        quicrNamespace(quicrNamespace),
        publishFlag(true)
    {
    }

    ~QPublicationDelegate() {
        std::cerr << "~QPublicationDelegate" << std::endl;
    }

public:
    virtual int prepare(const std::string& sourceId, const std::string& qualityProfile) = 0;
    virtual int update(const std::string& sourceId, const std::string& qualityProfile) = 0;
    virtual void publish(bool) = 0;

private:
    quicr::Namespace quicrNamespace;
    bool publishFlag;
};
class QSubscriberDelegate
{
public:
    virtual std::shared_ptr<QSubscriptionDelegate> allocateSubByNamespace(const quicr::Namespace& quicrNamespace, const std::string& qualityProfile) = 0;
    virtual int removeSubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
    ~QSubscriberDelegate() {
        std::cerr << "~QSubscriberDelegate" << std::endl;
    }
};
class QPublisherDelegate
{
public:
    virtual std::shared_ptr<QPublicationDelegate> allocatePubByNamespace(const quicr::Namespace& quicrNamespace, const std::string& sourceID, const std::string& qualityProfile) = 0;
    virtual int removePubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
    ~QPublisherDelegate() {
        std::cerr << "~QPublisherDelegate" << std::endl;
    }
};
}        // namespace qmedia

#endif   /* QDelegates_h */

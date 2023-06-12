#ifndef QDelegates_h
#define QDelegates_h

#include <quicr/quicr_namespace.h>
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
    virtual int subscribedObject(quicr::bytes&& data) = 0;
};
class QPublicationDelegate
{
public:
    QPublicationDelegate(const quicr::Namespace& quicrNamespace) :
        quicrNamespace(quicrNamespace),
        publishFlag(true)
    {
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
    virtual std::shared_ptr<QSubscriptionDelegate> allocateSubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
    virtual int removeSubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
};
class QPublisherDelegate
{
public:
    virtual std::shared_ptr<QPublicationDelegate> allocatePubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
    virtual int removePubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
};
}        // namespace qmedia

#endif   /* QDelegates_h */

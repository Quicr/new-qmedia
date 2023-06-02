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
    virtual int prepare(const std::string& sourceId, const std::string& label, const std::string& qualityProfile) = 0;
    virtual int update(const std::string& sourceId, const std::string& label, const std::string& qualityProfile) = 0;
    virtual int subscribedObject(quicr::bytes&& data) = 0;
    // virtual quicr::Namespace getNamespace() = 0;
};
class QPublicationDelegate
{
public:
    QPublicationDelegate(const quicr::Namespace& quicrNamespace) :
        groupId(0),
        objectId(0),
        priority(0),
        expiry(0),
        reliableTransport(false),
        quicrNamespace(quicrNamespace),
        publishFlag(true)
    {
    }

public:
    virtual int prepare(const std::string& sourceId, const std::string& qualityProfile) = 0;
    virtual int update(const std::string& sourceId, const std::string& qualityProfile) = 0;
    virtual void publish(bool) = 0;

    const quicr::Name object_id_mask = ~(~quicr::Name() << 16);
    const quicr::Name group_id_mask = ~(~quicr::Name() << 32) << 16;
    void publishNamedObject(std::shared_ptr<quicr::QuicRClient> quicrClient, std::uint8_t* data, std::size_t len)
    {
        if (quicrClient)
        {
            quicr::Name quicrName(quicrNamespace.name());
            quicrName = (0x0_name | ++groupId) << 16 | (quicrName & ~group_id_mask);
            quicrName = (0x0_name | ++objectId) | (quicrName & ~object_id_mask);
            std::cerr << "publishing " << quicrName.to_hex() << " len = " << len << std::endl;
            quicr::bytes b(data, data + len);
            quicrClient->publishNamedObject(quicrName, priority, expiry, reliableTransport, std::move(b));
        }
    }

private:
    std::uint32_t incrGroupId() { return ++groupId; }
    std::uint16_t incrObjectId() { return ++objectId; }

private:
    std::uint32_t groupId;
    std::uint16_t objectId;
    std::uint8_t priority;
    std::uint16_t expiry;
    bool reliableTransport;
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

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
        quicrNamespace(quicrNamespace),
        publishFlag(true)
    {
    }

public:
    virtual int prepare(const std::string& sourceId, const std::string& qualityProfile) = 0;
    virtual int update(const std::string& sourceId, const std::string& qualityProfile) = 0;
    virtual void publish(bool) = 0;

/*
    const quicr::Name object_id_mask = ~(~quicr::Name() << 16);
    const quicr::Name group_id_mask = ~(~quicr::Name() << 32) << 16;
    void publishNamedObject(std::shared_ptr<quicr::QuicRClient> quicrClient, std::uint8_t* data, std::size_t len, bool groupFlag)
    {
        if (quicrClient)
        {
            quicr::Name quicrName(quicrNamespace.name());
            if (groupFlag)
            {
                quicrName = (0x0_name | groupId) << 16 | (quicrName & ~group_id_mask);
                quicrName = (0x0_name | objectId) | (quicrName & ~object_id_mask);
                ++groupId;
                objectId = 0;
            }
            else
            {
                quicrName = (0x0_name | groupId) << 16 | (quicrName & ~group_id_mask);
                quicrName = (0x0_name | objectId) | (quicrName & ~object_id_mask);
                ++objectId;
            }
            std::cerr << "publish named object " << quicrName.to_hex() << std::endl;
            quicr::bytes b(data, data + len);
            quicrClient->publishNamedObject(quicrName, priority, expiry, reliableTransport, std::move(b));
        }
    }
*/
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

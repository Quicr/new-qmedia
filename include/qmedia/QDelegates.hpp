#ifndef QDelegates_h
#define QDelegates_h

#include "qmedia/ManifestTypes.hpp"
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

public:
    virtual int prepare(const std::string& sourceId,
                        const std::string& label,
                        const manifest::ProfileSet& profiles,
                        bool& reliable) = 0;
    virtual int update(const std::string& sourceId, const std::string& label, const manifest::ProfileSet& profiles) = 0;
    virtual int subscribedObject(const quicr::Namespace& quicrNamespace, quicr::bytes&& data, std::uint32_t groupId, std::uint16_t objectId) = 0;
};

class QPublicationDelegate
{
public:
    virtual int prepare(const std::string& sourceId, const std::string& qualityProfile, bool& reliable) = 0;
    virtual int update(const std::string& sourceId, const std::string& qualityProfile) = 0;
    virtual void publish(bool) = 0;
};

class QSubscriberDelegate
{
public:
    virtual std::shared_ptr<QSubscriptionDelegate> allocateSubBySourceId(const std::string& sourceId,
                                                                         const manifest::ProfileSet& profileSet) = 0;
    virtual int removeSubBySourceId(const std::string& sourceId) = 0;
};

class QPublisherDelegate
{
public:
    virtual std::shared_ptr<QPublicationDelegate> allocatePubByNamespace(const quicr::Namespace& quicrNamespace,
                                                                         const std::string& sourceID,
                                                                         const std::string& qualityProfilem,
                                                                         const std::string& appTags) = 0;
    virtual int removePubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
};
}        // namespace qmedia

#endif /* QDelegates_h */

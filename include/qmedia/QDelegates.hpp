#ifndef QDelegates_h
#define QDelegates_h

#include <quicr/namespace.h>
#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>
#include <string>
#include <iostream>

namespace qmedia
{
class SubscriberDelegate
{
public:
    virtual std::shared_ptr<class SubscriptionDelegate> allocateSubByNamespace(const quicr::Namespace& quicrNamespace,
                                                                               const std::string& qualityProfile,
                                                                               const cantina::LoggerPointer& logger) = 0;
    virtual int removeSubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
};

class PublisherDelegate
{
public:
    virtual std::shared_ptr<class PublicationDelegate>
    allocatePubByNamespace(const quicr::Namespace& quicrNamespace,
                           const std::string& sourceID,
                           const std::vector<std::uint8_t>& priorities,
                           std::uint16_t expiry,
                           const std::string& qualityProfile,
                           const cantina::LoggerPointer& logger) = 0;
    virtual int removePubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
};
}        // namespace qmedia

#endif /* QDelegates_h */

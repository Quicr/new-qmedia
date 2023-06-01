#ifndef QDelegates_h
#define QDelegates_h

#include <quicr/quicr_namespace.h>
#include <quicr/quicr_common.h>
#include <string>

namespace qmedia 
{
class QSubscriptionDelegate 
{
public:
    virtual int prepare(const std::string& sourceId,  const std::string& label, const std::string& qualityProfile) = 0;
    virtual int update(const std::string& sourceId,  const std::string& label, const std::string& qualityProfile) = 0;    
    virtual int subscribedObject(quicr::bytes&& data) = 0;
    //virtual quicr::Namespace getNamespace() = 0;    
};
class QPublicationDelegate
{
public:
    virtual int prepare(const std::string& sourceId,  const std::string& qualityProfile) = 0;
    virtual int update(const std::string& sourceId,  const std::string& qualityProfile) = 0;
    virtual void publish(bool) = 0;
    //virtual quicr::Namespace getNamespace() = 0;    
    // add ttl, priority
    //int publishObject(name, object, ttl, priority);
};
class QSubscriberDelegate
{
public:
    virtual std::shared_ptr<QSubscriptionDelegate>  allocateSubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
    virtual int removeSubByNamespace(const quicr::Namespace& quicrNamespace) = 0; 
};
class QPublisherDelegate
{
public:
    virtual std::shared_ptr<QPublicationDelegate> allocatePubByNamespace(const quicr::Namespace& quicrNamespace) = 0;
    virtual int removePubByNamespace(const quicr::Namespace& quicrNamespace) = 0; 
};
}

#endif /* QDelegates_h */

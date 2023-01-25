#pragma once

#include <string>
#include <functional>
#include <cstring>
#include <iostream>
#include <map>
#include <assert.h>

#include "callback.hh"
#include "logger.hh"

#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>

namespace qmedia
{

enum struct MediaDirection
{
    sendonly = 0,
    recvonly,
    sendrecv,
    unknown
};


class MediaTransportSubDelegate : public quicr::SubscriberDelegate
{
public:
    MediaTransportSubDelegate();

    virtual void onSubscribeResponse(const quicr::QUICRNamespace& quicr_namespace,
                                    const quicr::SubscribeResult& result);
    virtual void onSubscriptionEnded(const quicr::QUICRNamespace& quicr_namespace,
                                    const quicr::SubscribeResult& result);
private:
    bool canReciveSubs;
};

class MediaTransportPubDelegate : public quicr::PublisherDelegate
{
public:
    MediaTransportPubDelegate();

    virtual void onPublishIntentResponse(const quicr::QUICRNamespace& quicr_namespace,
                                        const quicr::PublishIntentResult& result);
private:
    bool canPublish;
};

// handy typedefs
using MediaStreamId = uint64_t;

// forward declarations
struct MediaStream;
struct MediaTransport;

class MediaClient
{
public:
    explicit MediaClient(const LoggerPointer &s = nullptr);

    // Stream API
    MediaStreamId add_object_stream(const quicr::QUICRName& quicr_name,
                                   MediaDirection media_direction);

    void remove_object_stream(MediaStreamId media_stream_id);

    // media apis
    void send_object(MediaStreamId streamId,
                    uint8_t *buffer,
                    unsigned int length,
                    uint64_t timestamp);

    // returns actual bytes filled
    std::uint32_t get_object(MediaStreamId streamId,
                  uint64_t &timestamp,
                  unsigned char **buffer);

    void release_media_buffer(void* buffer);

private:
    uint64_t get_next_id(void);

private:
    LoggerPointer log;

    // SAH - I really don't like these maps...
    // list of media streams
    std::map<MediaStreamId, quicr::QUICRName> active_stream;
    std::map<MediaStreamId, std::shared_ptr<MediaTransportSubDelegate>> active_subscription_delegates;
    std::map<MediaStreamId, std::shared_ptr<MediaTransportPubDelegate>> active_publish_delegates;

    // underlying media transport
    std::shared_ptr<quicr::QuicRClient> quicRClient;

    MediaStreamId streamId;
};

}        // namespace qmedia

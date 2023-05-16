#pragma once

#include <string>
#include <functional>
#include <cstring>
#include <iostream>
#include <map>
#include <assert.h>

#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>
#include <transport/logger.h>

#include <thread>
#include <mutex>
#include "../../src/basicLogger.h"

namespace qmedia
{

typedef void(* SubscribeCallback)(uint64_t id,
                                  uint8_t media_id,
                                  uint16_t client_id,
                                  uint8_t* data,
                                  uint32_t length,
                                  uint64_t timestamp);

using MediaStreamId = uint64_t;

class MediaTransportSubDelegate : public quicr::SubscriberDelegate
{
public:
    MediaTransportSubDelegate(MediaStreamId id, quicr::Namespace quicr_namespace, SubscribeCallback callback);

    virtual void onSubscribeResponse(const quicr::Namespace& quicr_namespace, const quicr::SubscribeResult& result);

    virtual void onSubscriptionEnded(const quicr::Namespace& quicr_namespace,
                                     const quicr::SubscribeResult::SubscribeStatus& result);

    virtual void onSubscribedObject(const quicr::Name& quicr_name,
                                    uint8_t priority,
                                    uint16_t expiry_age_ms,
                                    bool use_reliable_transport,
                                    quicr::bytes&& data);

    bool isActive() { return canReceiveSubs; }

private:
    bool canReceiveSubs;
    MediaStreamId id;
    [[maybe_unused]] quicr::Namespace quicr_namespace;
    SubscribeCallback callback;
};

class MediaTransportPubDelegate : public quicr::PublisherDelegate
{
public:
    MediaTransportPubDelegate(MediaStreamId id);
    virtual void onPublishIntentResponse(const quicr::Namespace& quicr_namespace,
                                         const quicr::PublishIntentResult& result);

private:
    // bool canPublish;
};

class MediaClient
{
private:
    struct MediaSubscription {
        const std::shared_ptr<MediaTransportSubDelegate> delegate;
        const quicr::Namespace quicr_namespace;
        const quicr::SubscribeIntent intent;
        const std::string origin_url;
        const bool use_reliable_transport;
        const std::string auth_token;
        quicr::bytes e2e_token;
    };

    struct MediaPublishIntent {
        const std::shared_ptr<MediaTransportPubDelegate> delegate;
        const quicr::Namespace quicr_namespace;
        const std::string auth_token;
    };

public:
    explicit MediaClient(const char* remote_address,
                         std::uint16_t remote_port,
                         quicr::RelayInfo::Protocol protocol);

    ~MediaClient();

    void close();

    void periodic_resubscribe(const unsigned int seconds);

    void add_raw_subscribe(const quicr::Namespace&, const std::shared_ptr<quicr::SubscriberDelegate>& delegate);
    MediaStreamId add_stream_subscribe(std::uint8_t media_type, SubscribeCallback callback);
    MediaStreamId add_audio_stream_subscribe(std::uint8_t media_type, SubscribeCallback callback);
    MediaStreamId add_video_stream_subscribe(std::uint8_t media_type, SubscribeCallback callback);

    MediaStreamId add_publish_intent(std::uint8_t media_type, std::uint16_t client_id);
    MediaStreamId add_audio_publish_intent(std::uint8_t media_type, std::uint16_t client_id);
    MediaStreamId add_video_publish_intent(std::uint8_t media_type, std::uint16_t client_id);

    void remove_publish(MediaStreamId streamId);
    void remove_video_publish(MediaStreamId streamId);
    void remove_audio_publish(MediaStreamId streamId);

    void remove_subscribe(MediaStreamId streamId);

    void send_raw(const quicr::Name &quicr_name, uint8_t* data, std::uint32_t length);
    void send_audio_media(MediaStreamId streamid, uint8_t* data, std::uint32_t length, std::uint64_t timestamp);
    void send_video_media(MediaStreamId streamid,
                          uint8_t* data,
                          std::uint32_t length,
                          std::uint64_t timestamp,
                          bool groupidflag = false);

private:
    std::map<MediaStreamId, std::shared_ptr<MediaSubscription>> subscriptions;

    std::map<MediaStreamId, std::shared_ptr<MediaPublishIntent>> publish_intents;
    std::map<MediaStreamId, quicr::Name> publications;

    basicLogger logger;
    std::shared_ptr<quicr::QuicRClient> quicRClient;

    MediaStreamId _streamId;

    std::mutex pubsub_mutex;
    std::thread keepalive_thread;
    bool stop;

    // SAH - these are temporary until `Manifests`
    const uint32_t _orgId;
    const uint8_t _appId;
    const uint32_t _confId;

    // SAH - don't like having to use `transport` logger
};

}        // namespace qmedia

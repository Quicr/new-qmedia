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

typedef void(CALL *SubscribeCallback)(uint64_t id, uint8_t media_id, uint16_t client_id, uint8_t *data, uint32_t length, uint64_t timestamp);

using MediaStreamId = uint64_t;

//typedef void(CALL *SubscribeCallback)(uint64_t id, uint8_t *data, uint32_t length);

class MediaTransportSubDelegate : public quicr::SubscriberDelegate
{
public:
    MediaTransportSubDelegate(MediaStreamId id, 
                            quicr::Namespace quicr_namespace, 
                            SubscribeCallback callback);

    virtual void onSubscribeResponse(const quicr::Namespace& quicr_namespace,
                                    const quicr::SubscribeResult::SubscribeStatus& result);

    virtual void onSubscriptionEnded(const quicr::Namespace& quicr_namespace,
                                    const quicr::SubscribeResult& result);

    virtual void onSubscribedObject(const quicr::Name& quicr_name,
                                    uint8_t priority,
                                    uint16_t expiry_age_ms,
                                    bool use_reliable_transport,
                                    quicr::bytes&& data);
                                   

    bool isActive() { return canReceiveSubs; }
private:
    bool canReceiveSubs;
    MediaStreamId id;
    quicr::Namespace quicr_namespace;
    SubscribeCallback callback;
};

class MediaTransportPubDelegate : public quicr::PublisherDelegate
{
public:
    MediaTransportPubDelegate(MediaStreamId id);
    virtual void onPublishIntentResponse(const quicr::Namespace& quicr_namespace,
                                        const quicr::PublishIntentResult& result);
private:
    //bool canPublish;
};



class MediaClient
{
public:
    explicit MediaClient(const char *remote_address,
                        std::uint16_t remote_port,
                        quicr::RelayInfo::Protocol protocol,
                        const LoggerPointer &s = nullptr);

    MediaStreamId add_audio_stream_subscribe(std::uint8_t codec_type,
                                             SubscribeCallback callback);
    MediaStreamId add_video_stream_subscribe(std::uint8_t codec_type,
                                             SubscribeCallback callback);
                                                                      
    MediaStreamId add_audio_publish_intent(std::uint8_t codec_type, std::uint16_t client_id);
    MediaStreamId add_video_publish_intent(std::uint8_t codec_type, std::uint16_t client_id);

    void remove_video_publish(MediaStreamId streamId);
    void remove_video_subscribe(MediaStreamId streamId);
    void remove_audio_publish(MediaStreamId streamId);
    void remove_audio_subscribe(MediaStreamId streamId);

    void send_audio_media(MediaStreamId streamid, uint8_t *data, std::uint32_t length, std::uint64_t timestamp);
    void send_video_media(MediaStreamId streamid, uint8_t *data, std::uint32_t length, std::uint64_t timestamp, bool groupidflag = false); 

private:
    LoggerPointer log;

    std::uint32_t _streamId;

    std::map<MediaStreamId, std::shared_ptr<MediaTransportSubDelegate>> active_subscription_delegates;
    std::map<MediaStreamId, std::shared_ptr<MediaTransportPubDelegate>> active_publish_delegates;

    std::map<MediaStreamId, quicr::Name> publish_names;
    
    std::shared_ptr<quicr::QuicRClient> quicRClientSubscribe;
    std::shared_ptr<quicr::QuicRClient> quicRClientPublish;

    const uint32_t _orgId;
    const uint8_t  _appId;
    const uint32_t _confId;

    qtransport::LogHandler logger;
};

}        // namespace qmedia

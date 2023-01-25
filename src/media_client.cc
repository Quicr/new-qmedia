#include <memory>
#include <sstream>

#include <qmedia/media_client.hh>

//#include "media_stream.hh"
//#include "packet.hh"

namespace qmedia
{


MediaTransportSubDelegate::MediaTransportSubDelegate() : canReciveSubs(false)
{

}

void MediaTransportSubDelegate::onSubscribeResponse(const quicr::QUICRNamespace& quicr_namespace,
                                   const quicr::SubscribeResult& result)
{
    if (result.status == quicr::SubscribeResult::SubscribeStatus::Ok)
canReciveSubs
}

void MediaTransportSubDelegate::onSubscriptionEnded(const quicr::QUICRNamespace& quicr_namespace,
                                   const quicr::SubscribeResult& result)
{
}

MediaTransportPubDelegate::MediaTransportPubDelegate() : canPublish(false)
{

}

void MediaTransportPubDelegate::onPublishIntentResponse(const quicr::QUICRNamespace& quicr_namespace,
                                       const quicr::PublishIntentResult& result)
{
    if (result.status == quicr::PublishStatus::Ok) {
        canPublish = true;
    } else {
        canPublish = false;
    }
}

MediaStreamId MediaClient::get_next_id()
{
    return ++streamId;
}

MediaClient::MediaClient(const LoggerPointer &parent_logger) :
    streamId(0), 
    log(std::make_shared<Logger>("qmedia", parent_logger))
{
   // quicRClient = std::make_unique<quicr::QuicRClient>()
/*
      QuicRClient(ITransport& transport,
              std::shared_ptr<SubscriberDelegate> subscriber_delegate,
              std::shared_ptr<PublisherDelegate> pub_delegate);*/
}

MediaStreamId MediaClient::add_object_stream(const quicr::QUICRName& quicr_name, 
                                            MediaDirection media_direction)
{
    // SAH - do verfication here...
    /// check to see if we already have a sub for this name?

    MediaStreamId streamId = get_next_id();
    active_stream[streamId] = quicr_name;

    if (media_direction == MediaDirection::recvonly ||
        media_direction == MediaDirection::sendrecv)
    {
        // create a subscibe delegate
        active_subscription_delegates[streamId] = std::make_shared<MediaTransportSubDelegate>();
    }

    if (media_direction == MediaDirection::sendonly ||
        media_direction == MediaDirection::sendrecv)
    {
       // create a publish delegatge
        active_publish_delegates[streamId] = std::make_shared<MediaTransportPubDelegate>();
    }

    log->info << "[MediaClient::add_stream]: created: "
              << streamId << std::flush;
    return streamId;
}


// media apis
void MediaClient::send_object(MediaStreamId streamId,
                             uint8_t *buffer,
                             unsigned int length,
                             uint64_t timestamp)
{
    // find the mapping to quicr name / ?

    // call quicRClient->publishNamedObject....
    /*
    QuicRClient::publishNamedObject(const QUICRName& quicr_name,
                                    uint8_t priority,
                                    uint16_t expiry_age_ms,
                                    bool use_reliable_transport,
                                    bytes&& data)
                                    */
}


std::uint32_t MediaClient::get_object(MediaStreamId streamId,
                           uint64_t &timestamp,
                           unsigned char **buffer)
{
    // audio from audio queue
    // included in the data is the timestamp the the encoded data
}

void MediaClient::remove_object_stream(MediaStreamId media_stream_id)
{

}

void MediaClient::release_media_buffer(void *buffer)
{
    ///delete((buffer);
}

}        // namespace qmedia

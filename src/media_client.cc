#pragma once
#include <memory>

#include <qmedia/media_client.hh>
#include "media_transport.hh"
#include "media_stream.hh"

namespace qmedia
{

void MediaClient::init_transport(TransportType /*transport_type*/,
                                 const std::string &remote_address,
                                 unsigned int remote_port)
{
    media_transport = std::make_shared<MediaTransport>(remote_address,
                                                       remote_port);
}

MediaStreamId MediaClient::add_audio_stream(uint64_t domain,
                                            uint64_t conference_id,
                                            uint64_t client_id,
                                            const AudioConfig &media_config)
{
    // create a new media stream and associate the transport
    auto media_stream = MediaStreamFactory::create_audio_stream(
        domain, conference_id, client_id, media_config);
    media_stream->set_transport(media_transport);
    active_streams[media_stream->id()] = media_stream;
}

MediaStreamId MediaClient::add_video_stream(uint64_t domain,
                                            uint64_t conference_id,
                                            uint64_t client_id,
                                            const VideoConfig &media_config)
{
    // create a new media stream and associate the transport
    auto media_stream = MediaStreamFactory::create_video_stream(
        domain, conference_id, client_id, media_config);
    media_stream->set_transport(media_transport);
    active_streams[media_stream->id()] = media_stream;
}

// media apis
void MediaClient::send_audio(MediaStreamId streamId,
                             uint8_t *buffer,
                             unsigned int length,
                             uint64_t timestamp)
{
    // happens on client thread
    if (!active_streams.count(streamId))
    {
        // log and return
        return;
    }

    auto audio_stream = std::dynamic_pointer_cast<AudioStream>(active_streams[streamId]);
    if (audio_stream) {
        // revisit this
        MediaConfig config;
        audio_stream->handle_media(buffer, length, timestamp, config);
    }

}

void MediaClient::send_video(MediaStreamId streamId,
                             uint8_t *buffer,
                             uint32_t length,
                             uint32_t width,
                             uint32_t height,
                             uint32_t stride_y,
                             uint32_t stride_uv,
                             uint32_t offset_u,
                             uint32_t offset_v,
                             uint32_t format,
                             uint64_t timestamp)
{
    // happens on client thread
    if (!active_streams.count(streamId))
    {
        // log and return
        return;
    }

    auto video_stream = std::dynamic_pointer_cast<VideoStream>(active_streams[streamId]);
    if (video_stream) {
        // revisit this
        MediaConfig config;
        video_stream->handle_media(buffer, length, timestamp, config);
    }
}


void MediaClient::do_work()
{
    // Wait on a condition variable
    while (!shutdown)
    {

        auto messages = std::vector<TransportMessageInfo>{};
        media_transport->check_network_messages(messages);

        if (messages.empty())
        {
            continue;
        }

        for (auto& message: messages) {
            // hand the data to appropriate media stream
            auto media_stream_id = std::stoi(message.name);
            //  Note: since a subscribe should preceed before
            // data arrive, we should find an entry when
            // there is data for a given stream
            if(!active_streams.count(media_stream_id)) {
                log->warning << media_stream_id << " not found, ignoring data" << std::flush;
                continue;
            }

            active_streams[media_stream_id]->

        }

        if (Packet::Type::StreamContent == packet->packetType)
        {
            uint64_t clientID = packet->clientID;
            uint64_t sourceID = packet->sourceID;
            uint64_t sourceTS = packet->sourceRecordTime;
            // TODO: This is pre-decode media type, which is
            // not really right but close enough. Hold off changing a lot
            // to get at that data before Geir's changes to Jitter/Playout.
            Packet::MediaType sourceType = packet->mediaType;
            bool new_stream;
            auto jitter_instance = getJitter(clientID);
            if (jitter_instance == nullptr)
            {
                jitter_instance = createJitter(clientID);
            }

            if (jitter_instance != nullptr)
            {
                new_stream = jitter_instance->push(std::move(packet));
                // jitter assembles packets to frames, decodes, conceals
                // and makes frames available to client
                if (new_stream && newSources)
                {
                    newSources(clientID, sourceID, sourceTS, sourceType);
                }
            }
        }
        else if (Packet::Type::IdrRequest == packet->packetType)
        {
            log->info << "Received IDR request" << std::flush;
            reqKeyFrame = true;
        }
    }
}
}        // namespace qmedia

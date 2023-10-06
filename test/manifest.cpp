#include <doctest/doctest.h>

#include <qmedia/ManifestTypes.hpp>

using namespace qmedia::manifest;

static const auto manifest_json = std::string(R"(
{
  "clientId": "d85e3efb-abaa-4f7e-81b3-56f7cbdd4e0d",
  "subscriptions": [{
    "mediaType": "video",
    "sourceName": "Camera A1",
    "sourceId": "80a6ac22-f5a1-406d-bd48-b4c29ff6204b",
    "label": "Meeting Participant A",
    "profileSet": {
      "type": "singleordered",
      "profiles": [{
        "qualityProfile": "h264,width=1920,height=1080,fps=30,br=2000",
        "quicrNamespaceUrl": "quicr://webex.cisco.com/conferences/13/mediatype/192/endpoint/1"
      }, {
        "qualityProfile": "h264,width=1280,height=720,fps=30,br=1000",
        "quicrNamespaceUrl": "quicr://webex.cisco.com/conferences/13/mediatype/193/endpoint/1"
      }, {
        "qualityProfile": "h264,width=640,height=360,fps=20,br=500",
        "quicrNamespaceUrl": "quicr://webex.cisco.com/conferences/13/mediatype/194/endpoint/1"
      }]
    }
  }, {
    "mediaType": "audio",
    "sourceName": "Audio A1",
    "sourceId": "68ff975e-17e2-4960-b900-8b74f3e1da85",
    "label": "Meeting Participant A",
    "profileSet": {
      "type": "singleordered",
      "profiles": [{
        "qualityProfile": "opus,br=6",
        "quicrNamespaceUrl": "quicr://webex.cisco.com/conferences/13/mediatype/1/endpoint/1"
      }]
    }
  }],
  "publications": [{
    "mediaType": "video",
    "sourceName": "Camera A1",
    "sourceId": "80a6ac22-f5a1-406d-bd48-b4c29ff6204b",
    "label": "Meeting Participant A",
    "profileSet": {
      "type": "simulcast",
      "profiles": [{
        "qualityProfile": "h264,width=1920,height=1080,fps=30,br=2000",
        "expiry": 500,
        "priorities": [6, 7],
        "quicrNamespaceUrl": "quicr://webex.cisco.com/conferences/13/mediatype/192/endpoint/1"
      }, {
        "qualityProfile": "h264,width=1280,height=720,fps=30,br=1000",
        "expiry": 500,
        "priorities": [4, 5],
        "quicrNamespaceUrl": "quicr://webex.cisco.com/conferences/13/mediatype/193/endpoint/1"
      }, {
        "qualityProfile": "h264,width=640,height=360,fps=20,br=500",
        "expiry": 500,
        "priorities": [2, 3],
        "quicrNamespaceUrl": "quicr://webex.cisco.com/conferences/13/mediatype/194/endpoint/1"
      }]
    }
  }, {
    "mediaType": "audio",
    "sourceName": "Audio A1",
    "sourceId": "68ff975e-17e2-4960-b900-8b74f3e1da85",
    "label": "Meeting Participant A",
    "profileSet": {
      "type": "simulcast",
      "profiles": [{
        "qualityProfile": "opus,br=6",
        "expiry": 500,
        "priorities": [1],
        "quicrNamespaceUrl": "quicr://webex.cisco.com/conferences/13/mediatype/1/endpoint/1"
      }]
    }
  }],
  "urlTemplates": [
    "quicr://webex.cisco.com<pen=1><sub_pen=1>/conferences/<int24>/mediatype/<int8>/endpoint/<int16>"
  ]
})");

// Quicr namespaces encoded according to the URL template in the manifest
static const auto ns_vid_a_1 = quicr::Namespace(0x0000010100000dc00001000000000000_name, 80);
static const auto ns_vid_a_2 = quicr::Namespace(0x0000010100000dc10001000000000000_name, 80);
static const auto ns_vid_a_3 = quicr::Namespace(0x0000010100000dc20001000000000000_name, 80);
static const auto ns_aud_a_1 = quicr::Namespace(0x0000010100000d010001000000000000_name, 80);

static const auto expected_manifest_obj = Manifest{
    .subscriptions = {{.mediaType = "video",
                       .sourceName = "Camera A1",
                       .sourceId = "80a6ac22-f5a1-406d-bd48-b4c29ff6204b",
                       .label = "Meeting Participant A",
                       .profileSet = {.type = "singleordered",
                                      .profiles = {{.qualityProfile = "h264,width=1920,height=1080,fps=30,br=2000",
                                                    .quicrNamespace = ns_vid_a_1},
                                                   {.qualityProfile = "h264,width=1280,height=720,fps=30,br=1000",
                                                    .quicrNamespace = ns_vid_a_2},
                                                   {.qualityProfile = "h264,width=640,height=360,fps=20,br=500",
                                                    .quicrNamespace = ns_vid_a_3}}}},
                      {.mediaType = "audio",
                       .sourceName = "Audio A1",
                       .sourceId = "68ff975e-17e2-4960-b900-8b74f3e1da85",
                       .label = "Meeting Participant A",
                       .profileSet = {.type = "singleordered",
                                      .profiles = {{.qualityProfile = "opus,br=6",
                                                    .quicrNamespace = ns_aud_a_1}}}}},
    .publications = {{.mediaType = "video",
                       .sourceName = "Camera A1",
                       .sourceId = "80a6ac22-f5a1-406d-bd48-b4c29ff6204b",
                       .label = "Meeting Participant A",
                       .profileSet = {.type = "simulcast",
                                      .profiles = {{.qualityProfile = "h264,width=1920,height=1080,fps=30,br=2000",
                                                    .quicrNamespace = ns_vid_a_1,
                                                    .priorities = {6, 7},
                                                    .expiry = 500},
                                                   {.qualityProfile = "h264,width=1280,height=720,fps=30,br=1000",
                                                    .quicrNamespace = ns_vid_a_2,
                                                    .priorities = {4, 5},
                                                    .expiry = 500},
                                                   {.qualityProfile = "h264,width=640,height=360,fps=20,br=500",
                                                    .quicrNamespace = ns_vid_a_3,
                                                    .priorities = {2, 3},
                                                    .expiry = 500}}}},
                      {.mediaType = "audio",
                       .sourceName = "Audio A1",
                       .sourceId = "68ff975e-17e2-4960-b900-8b74f3e1da85",
                       .label = "Meeting Participant A",
                       .profileSet = {.type = "simulcast",
                                      .profiles = {{.qualityProfile = "opus,br=6",
                                                    .quicrNamespace = ns_aud_a_1,
                                                    .priorities = {1},
                                                    .expiry = 500}}}}}};

TEST_CASE("Manifest parsing")
{
    const auto actual_manifest_obj = json::parse(manifest_json).get<Manifest>();
    REQUIRE(actual_manifest_obj == expected_manifest_obj);
}

#include <doctest/doctest.h>

#include <qmedia/ManifestTypes.hpp>

using namespace qmedia::manifest;

static const auto manifest_json = std::string(R"(
{
  "clientId": "d85e3efb-abaa-4f7e-81b3-56f7cbdd4e0d",
  "subscriptions": [{
    "mediaType": "video",
    "sourceName": "Macbook camera",
    "sourceId": "80a6ac22-f5a1-406d-bd48-b4c29ff6204b",
    "label": "Richard B..",
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
    "sourceName": "Macbook mic",
    "sourceId": "68ff975e-17e2-4960-b900-8b74f3e1da85",
    "label": "Richard B..",
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
    "sourceName": "Macbook camera",
    "sourceId": "80a6ac22-f5a1-406d-bd48-b4c29ff6204b",
    "label": "Richard B..",
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
    "sourceName": "Macbook mic",
    "sourceId": "68ff975e-17e2-4960-b900-8b74f3e1da85",
    "label": "Richard B..",
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

static const auto expected_manifest_obj = Manifest{
    .subscriptions = {{.mediaType = "video",
                       .sourceName = "Macbook camera",
                       .sourceId = "80a6ac22-f5a1-406d-bd48-b4c29ff6204b",
                       .label = "Richard B..",
                       .profileSet = {.type = "singleordered",
                                      .profiles = {{.qualityProfile = "h264,width=1920,height=1080,fps=30,br=2000",
                                                    .quicrNamespaceUrl = "quicr://webex.cisco.com/conferences/13/"
                                                                         "mediatype/192/endpoint/1"},
                                                   {.qualityProfile = "h264,width=1280,height=720,fps=30,br=1000",
                                                    .quicrNamespaceUrl = "quicr://webex.cisco.com/conferences/13/"
                                                                         "mediatype/193/endpoint/1"},
                                                   {.qualityProfile = "h264,width=640,height=360,fps=20,br=500",
                                                    .quicrNamespaceUrl = "quicr://webex.cisco.com/conferences/13/"
                                                                         "mediatype/194/endpoint/1"}}}},
                      {.mediaType = "audio",
                       .sourceName = "Macbook mic",
                       .sourceId = "68ff975e-17e2-4960-b900-8b74f3e1da85",
                       .label = "Richard B..",
                       .profileSet = {.type = "singleordered",
                                      .profiles = {{.qualityProfile = "opus,br=6",
                                                    .quicrNamespaceUrl = "quicr://webex.cisco.com/conferences/13/"
                                                                         "mediatype/1/endpoint/1"}}}}},
    .publications = {{.mediaType = "video",
                      .sourceName = "Macbook camera",
                      .sourceId = "80a6ac22-f5a1-406d-bd48-b4c29ff6204b",
                      .label = "Richard B..",
                      .profileSet = {.type = "simulcast",
                                     .profiles = {{.qualityProfile = "h264,width=1920,height=1080,fps=30,br=2000",
                                                   .quicrNamespaceUrl = "quicr://webex.cisco.com/conferences/13/"
                                                                        "mediatype/192/endpoint/1",
                                                   .priorities = {6, 7},
                                                   .expiry = 500},
                                                  {.qualityProfile = "h264,width=1280,height=720,fps=30,br=1000",
                                                   .quicrNamespaceUrl = "quicr://webex.cisco.com/conferences/13/"
                                                                        "mediatype/193/endpoint/1",
                                                   .expiry = 500,
                                                   .priorities = {4, 5}},
                                                  {.qualityProfile = "h264,width=640,height=360,fps=20,br=500",
                                                   .quicrNamespaceUrl = "quicr://webex.cisco.com/conferences/13/"
                                                                        "mediatype/194/endpoint/1",
                                                   .expiry = 500,
                                                   .priorities = {2, 3}}}}},
                     {.mediaType = "audio",
                      .sourceName = "Macbook mic",
                      .sourceId = "68ff975e-17e2-4960-b900-8b74f3e1da85",
                      .label = "Richard B..",
                      .profileSet = {.type = "simulcast",
                                     .profiles = {{.qualityProfile = "opus,br=6",
                                                   .expiry = 500,
                                                   .priorities = {1},
                                                   .quicrNamespaceUrl = "quicr://webex.cisco.com/conferences/13/"
                                                                        "mediatype/1/endpoint/1"}}}}},
    .urlTemplates = {"quicr://webex.cisco.com<pen=1><sub_pen=1>/conferences/<int24>/mediatype/<int8>/endpoint/"
                     "<int16>"}};

TEST_CASE("Manifest parsing")
{
    const auto actual_manifest_obj = json::parse(manifest_json).get<Manifest>();
    REQUIRE(actual_manifest_obj == expected_manifest_obj);
}

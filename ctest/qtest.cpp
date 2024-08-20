#include <iostream>
#include <qmedia/QController.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <qmedia/ManifestTypes.hpp>
#include <qmedia/QDelegates.hpp>
#include <qmedia/QuicrDelegates.hpp>

#include <quicr/namespace.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include <chrono>
#include <thread>

std::vector<std::uint64_t> timeBuckets;

uint64_t timeSinceEpochMillisec() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static uint64_t then = 0;

class QSubscriptionTestDelegate : public qmedia::QSubscriptionDelegate
{
public:
    QSubscriptionTestDelegate(const SourceId& sourceId) :
        logger(spdlog::stderr_color_mt("TEST"))
    {
        SPDLOG_LOGGER_INFO(logger, "QSubscriptionTestDelegate constructed");
    }
public:
    int prepare(const std::string& sourceId, const std::string& label, const qmedia::manifest::ProfileSet& profileSet, quicr::TransportMode& transportMode) override {
        transportMode = quicr::TransportMode::ReliablePerGroup;
        SPDLOG_LOGGER_INFO(logger, "QSubscriptionTestDelegate::prepare");
        return 0;
    }
    int update(const std::string& sourceId, const std::string& label, const qmedia::manifest::ProfileSet& profileSet) override {
        SPDLOG_LOGGER_INFO(logger, "QSubscriptionTestDelegate::update");
        return 1; //1 = needs prepare
    }

    int subscribedObject(const quicr::Namespace& quicrNamespace, quicr::bytes&& data, std::uint32_t groupId, std::uint16_t objectId) override {
        uint64_t now = timeSinceEpochMillisec();
        std::cerr << "groupId = " << groupId << std::endl;
        uint64_t then =  timeBuckets[groupId-1];
        std::cerr << "now = " << now << std::endl;
        std::cerr << "then = " << then << std::endl;
        uint64_t duration = now - then;
        std::cerr << "duration = " << duration << std::endl;
        std::cerr << "subscribedObject: data size " << data.size() << std::endl;
        std::cerr << "\tgroupId: " << groupId << std::endl;
        std::cerr << "\tobjectId: " << objectId << std::endl;
        return 0;
    }

private:
    const std::shared_ptr<spdlog::logger> logger;
};

class QSubsciberTestDelegate : public qmedia::QSubscriberDelegate
{
public:
    QSubsciberTestDelegate() : logger(spdlog::stderr_color_mt("TEST"))
    {
    }

    std::shared_ptr<qmedia::QSubscriptionDelegate> allocateSubBySourceId(const SourceId& sourceId, const qmedia::manifest::ProfileSet& profileSet)
    {
       SPDLOG_LOGGER_INFO(logger, "QSubscriberTestDelegate::allocateSubBySourceId");
       return std::make_shared<QSubscriptionTestDelegate>(sourceId);
    }

    int removeSubBySourceId(const SourceId& sourceId)
    {
       SPDLOG_LOGGER_INFO(logger, "QSubscriberTestDelegate::removeSubBySourceId");
       return 0;
    }

private:
    const std::shared_ptr<spdlog::logger> logger;
};

////// PUBLISH

class QPublicationTestDelegate : public qmedia::QPublicationDelegate
{
public:
    QPublicationTestDelegate(const quicr::Namespace& quicrNamespace) :
        qmedia::QPublicationDelegate(),
        logger(spdlog::stderr_color_mt("TEST"))
    //quicrNamespace(quicrNamespace)
    {
        SPDLOG_LOGGER_INFO(logger, "QPublicationTestDelegate constructed");
    }
public:
    int prepare(const std::string& sourceId,  const std::string& qualityProfile, quicr::TransportMode& transportMode)  {
        transportMode = quicr::TransportMode::ReliablePerGroup;
        SPDLOG_LOGGER_INFO(logger, "QPublicationTestDelegate::prepare");
        std::cerr << "pub prepare " << std::endl;
        return 0;
    }
    int update(const std::string& sourceId, const std::string& qualityProfile) {
        SPDLOG_LOGGER_INFO(logger, "QPublicationTestDelegate::update");
        return 1; //1 = needs prepare
    }
    void publish(bool pubFlag) {
        SPDLOG_LOGGER_INFO(logger, "QPublicationTestDelegate::publish");
    }

private:
    const std::shared_ptr<spdlog::logger> logger;
    quicr::Namespace quicrNamespace;
};

class QPubisherTestDelegate : public qmedia::QPublisherDelegate
{
public:
    QPubisherTestDelegate() : logger(spdlog::stderr_color_mt("TEST"))
    {
        SPDLOG_LOGGER_INFO(logger, "QPubisherTestDelegate constructed");
    }

    std::shared_ptr<qmedia::QPublicationDelegate> allocatePubByNamespace(const quicr::Namespace& quicrNamespace, const std::string& sourceID, const std::string& qualityProfile, const std::string& appTag)
    {
       SPDLOG_LOGGER_INFO(logger, "QPubisherTestDelegate::allocatePubByNamespace");
       std::cerr << "allocatePubByNamespace " << quicrNamespace << std::endl;
       return std::make_shared<QPublicationTestDelegate>(quicrNamespace);
    }

    int removePubByNamespace(const quicr::Namespace& quicrNamespace)
    {
       SPDLOG_LOGGER_INFO(logger, "QPubisherTestDelegate::removeByNamespace");
       return 0;
    }

private:
    const std::shared_ptr<spdlog::logger> logger;
};

int test()
{
   //qmedia::basicLogger logger;
    quicr::Namespace quicrNamespace;

    auto qSubscriber = std::make_shared<QSubsciberTestDelegate>();
    auto qPublisher = std::make_shared<QPubisherTestDelegate>();
    auto logger = spdlog::stderr_color_mt("QTEST");
    auto qController = std::make_shared<qmedia::QController>(qSubscriber, qPublisher, logger);

    //SPDLOG_LOGGER_INFO(logger, "connecting to qController");
    then = timeSinceEpochMillisec();
    qtransport::TransportConfig config {
        .tls_cert_filename = NULL,
        .tls_key_filename = NULL,
        .time_queue_init_queue_size = 200,
    };
    qController->connect("a@cisco.com", "127.0.0.1", 33435, quicr::RelayInfo::Protocol::QUIC, 0, config);
    //qController->connect("a@cisco.com", "relay.us-west-2.quicr.ctgpoc.com", 33437, quicr::RelayInfo::Protocol::QUIC);
    std::uint64_t now = timeSinceEpochMillisec();
    std::cerr << "connect duration " << now - then << std::endl;

    std::ifstream f("/tmp/manifest.json");

    // load file into string
    std::stringstream strStream;
    strStream << f.rdbuf();
    auto manifest = json::parse(strStream.str()).get<qmedia::manifest::Manifest>();

    then = timeSinceEpochMillisec();
    qController->updateManifest(manifest);
    now = timeSinceEpochMillisec();
    std::cerr << "update manifest  duration " << now - then << std::endl;
    std::uint8_t *data = new std::uint8_t[256];

    int num_buckets = 100;

    timeBuckets.resize(num_buckets);

    for (int i=0; i<num_buckets; i++)
    {
        timeBuckets[i] = 0;
    }

    std::cerr << "PUBLISH DATE ------------------------------- " << std::endl;
    for (int i=0; i<num_buckets; i++)
    {
        then = timeSinceEpochMillisec();
        std::cerr << "pub index = i " << i << " time : " << then << std::endl;
        timeBuckets[i] = then;
        qController->publishNamedObjectTest(data, 256, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cerr << "PUBLISH DATE -------------------------------  END " << std::endl;
    std::uint64_t tear_then  = timeSinceEpochMillisec();
    delete[] data;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    qController = nullptr;
    std::uint64_t tear_now  = timeSinceEpochMillisec();
    std::cerr << "TEST() ---- finished......tear duration: " << tear_now - tear_then << std::endl;
    return 0;
}

int main(int /*argc*/, char** /*arg*/)
{
   for (int i=0; i<100; ++i)
   {
    test();
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(120));
}

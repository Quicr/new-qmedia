#include <iostream>
#include <qmedia/QController.hpp>
#include <cantina/logger.h>

#include <qmedia/QDelegates.hpp>
#include <qmedia/QuicrDelegates.hpp>

#include <quicr/namespace.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include <chrono>
#include <thread>

std::vector<std::uint64_t> timeBuckets;

uint64_t timeSinceEpochMillisec()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static uint64_t then = 0;

class SubscriptionTestDelegate : public qmedia::SubscriptionDelegate
{
public:
    SubscriptionTestDelegate(const quicr::Namespace& quicrNamespace, const cantina::LoggerPointer& logger) :
        qmedia::SubscriptionDelegate(quicrNamespace, logger)
    {
        logger->Log("SubscriptionTestDelegate constructed");
    }

public:
    int prepare(const std::string& label, const std::string& qualityProfile, bool& reliable) override
    {
        logger->Log("SubscriptionTestDelegate::prepare");
        return 0;
    }

    int update(const std::string& label, const std::string& qualityProfile) override
    {
        logger->Log("SubscriptionTestDelegate::update");
        return 1;        // 1 = needs prepare
    }

    int subscribedObject(quicr::bytes&& data, std::uint32_t groupId, std::uint16_t objectId) override
    {
        uint64_t now = timeSinceEpochMillisec();
        std::cerr << "groupId = " << groupId << std::endl;
        uint64_t then = timeBuckets[groupId - 1];
        std::cerr << "now = " << now << std::endl;
        std::cerr << "then = " << then << std::endl;
        uint64_t duration = now - then;
        std::cerr << "duration = " << duration << std::endl;
        std::cerr << "subscribedObject: data size " << data.size() << std::endl;
        std::cerr << "\tgroupId: " << groupId << std::endl;
        std::cerr << "\tobjectId: " << objectId << std::endl;
        return 0;
    }
};

class SubscriberTestDelegate : public qmedia::SubscriberDelegate
{
public:
    SubscriberTestDelegate(const cantina::LoggerPointer& logger) :
        logger(std::make_shared<cantina::Logger>("STDEL", logger))
    {
    }

    std::shared_ptr<qmedia::SubscriptionDelegate> allocateSubByNamespace(const quicr::Namespace& quicrNamespace,
                                                                         const std::string& qualityProfile,
                                                                         const cantina::LoggerPointer& logger)
    {
        logger->Log("SubscriberTestDelegate::allocateSubByNamespace");
        return std::make_shared<SubscriptionTestDelegate>(quicrNamespace, logger);
    }

    int removeSubByNamespace(const quicr::Namespace& quicrNamespace)
    {
        logger->Log("SubscriberTestDelegate::removeByNamespace");
        return 0;
    }

private:
    const cantina::LoggerPointer logger;
};

////// PUBLISH

class PublicationTestDelegate : public qmedia::PublicationDelegate
{
public:
    PublicationTestDelegate(const quicr::Namespace& quicrNamespace, const cantina::LoggerPointer& logger) :
        qmedia::PublicationDelegate("", quicrNamespace, {}, 0, logger)
    {
        logger->Log("PublicationTestDelegate constructed");
    }

public:
    int prepare(const std::string& qualityProfile, bool& reliable) override
    {
        logger->Log("PublicationTestDelegate::prepare");
        std::cerr << "pub prepare " << std::endl;
        return 0;
    }

    int update(const std::string& qualityProfile) override
    {
        logger->Log("PublicationTestDelegate::update");
        return 1;        // 1 = needs prepare
    }

    void publish(bool pubFlag) override { logger->Log("PublicationTestDelegate::publish"); }
};

class PublisherTestDelegate : public qmedia::PublisherDelegate
{
public:
    PublisherTestDelegate(const cantina::LoggerPointer& logger) :
        logger(std::make_shared<cantina::Logger>("PTDEL", logger))
    {
        logger->Log("PublisherTestDelegate constructed");
    }

    std::shared_ptr<qmedia::PublicationDelegate> allocatePubByNamespace(const quicr::Namespace& quicrNamespace,
                                                                        const std::string& sourceID,
                                                                        const std::vector<std::uint8_t>& priorities,
                                                                        std::uint16_t expiry,
                                                                        const std::string& qualityProfile,
                                                                        const cantina::LoggerPointer& logger) override
    {
        logger->Log("PublisherTestDelegate::allocatePubByNamespace");
        std::cerr << "allocatePubByNamespace " << quicrNamespace << std::endl;
        return std::make_shared<PublicationTestDelegate>(quicrNamespace, logger);
    }

    int removePubByNamespace(const quicr::Namespace& quicrNamespace) override
    {
        logger->Log("PublisherTestDelegate::removeByNamespace");
        return 0;
    }

private:
    cantina::LoggerPointer logger;
};

int test()
{
    // qmedia::basicLogger logger;
    quicr::Namespace quicrNamespace;

    auto logger = std::make_shared<cantina::Logger>("QTest", "QTEST");
    auto qSubscriber = std::make_shared<SubscriberTestDelegate>(logger);
    auto qPublisher = std::make_shared<PublisherTestDelegate>(logger);
    auto qController = std::make_shared<qmedia::QController>(qSubscriber, qPublisher, logger);

    // logger->Log("connecting to qController");
    then = timeSinceEpochMillisec();
    qtransport::TransportConfig config{
        .tls_cert_filename = NULL,
        .tls_key_filename = NULL,
        .time_queue_init_queue_size = 200,
    };
    qController->connect("127.0.0.1", 33435, quicr::RelayInfo::Protocol::QUIC, config);
    // qController->connect("relay.us-west-2.quicr.ctgpoc.com", 33437, quicr::RelayInfo::Protocol::QUIC);
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
    std::uint8_t* data = new std::uint8_t[256];

    int num_buckets = 100;

    timeBuckets.resize(num_buckets);

    for (int i = 0; i < num_buckets; i++)
    {
        timeBuckets[i] = 0;
    }

    std::cerr << "PUBLISH DATE ------------------------------- " << std::endl;
    for (int i = 0; i < num_buckets; i++)
    {
        then = timeSinceEpochMillisec();
        std::cerr << "pub index = i " << i << " time : " << then << std::endl;
        timeBuckets[i] = then;
        qController->publishNamedObjectTest(data, 256, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cerr << "PUBLISH DATE -------------------------------  END " << std::endl;
    std::uint64_t tear_then = timeSinceEpochMillisec();
    delete[] data;
    qController = nullptr;
    std::uint64_t tear_now = timeSinceEpochMillisec();
    std::cerr << "TEST() ---- finished......tear duration: " << tear_now - tear_then << std::endl;
    return 0;
}

int main(int /*argc*/, char** /*arg*/)
{
    for (int i = 0; i < 100; ++i)
    {
        test();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
}

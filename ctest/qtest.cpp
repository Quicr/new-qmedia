#include <iostream>
#include <qmedia/QController.hpp>
#include <transport/logger.h>
#include "basicLogger.h"

#include <qmedia/QDelegates.hpp>
#include <qmedia/QuicrDelegates.hpp>

#include <quicr/quicr_namespace.h>

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
    QSubscriptionTestDelegate(const quicr::Namespace& quicrNamespace) : quicrNamespace(quicrNamespace)
    {
        logger.log(qtransport::LogLevel::info, "QSubscriptionTestDelegate constructed");
    }
public:
    int prepare(const std::string& sourceId,  const std::string& label, const std::string& qualityProfile) override {
        logger.log(qtransport::LogLevel::info, "QSubscriptionTestDelegate::prepare");
        return 0;
    }
    int update(const std::string& sourceId,  const std::string& label, const std::string& qualityProfile) override {
        logger.log(qtransport::LogLevel::info, "QSubscriptionTestDelegate::update");
        return 1; //1 = needs prepare
    }

    int subscribedObject(quicr::bytes&& data, std::uint32_t groupId, std::uint16_t objectId) override {
        uint64_t now = timeSinceEpochMillisec();

        uint64_t then =  timeBuckets[groupId-1];

        uint64_t duration = now - then;

        std::cerr << "duration = " << duration << std::endl;
        std::cerr << "subscribedObject: data size " << data.size() << std::endl;
        std::cerr << "\tgroupId: " << groupId << std::endl;
        std::cerr << "\tobjectId: " << objectId << std::endl;
        return 0;
    }

private:
    quicr::Namespace quicrNamespace;
    qmedia::basicLogger logger;
};

class QSubsciberTestDelegate : public qmedia::QSubscriberDelegate
{
public:
    QSubsciberTestDelegate() 
    {
    }

    std::shared_ptr<qmedia::QSubscriptionDelegate> allocateSubByNamespace(const quicr::Namespace& quicrNamespace, const std::string& qualityProfile)
    {
       logger.log(qtransport::LogLevel::info, "QSubscriberTestDelegate::allocateSubByNamespace");        
       return std::make_shared<QSubscriptionTestDelegate>(quicrNamespace);
    }

    int removeSubByNamespace(const quicr::Namespace& quicrNamespace) 
    {
       logger.log(qtransport::LogLevel::info, "QSubscriberTestDelegate::removeByNamespace"); 
     0;  return 0;
    }

private:
    qmedia::basicLogger logger;
};

////// PUBLISH

class QPublicationTestDelegate : public qmedia::QPublicationDelegate
{
public:
    QPublicationTestDelegate(const quicr::Namespace& quicrNamespace) : qmedia::QPublicationDelegate(quicrNamespace)
    //quicrNamespace(quicrNamespace)
    {
        logger.log(qtransport::LogLevel::info, "QPublicationTestDelegate constructed");
    }
public:
    int prepare(const std::string& sourceId,  const std::string& qualityProfile)  {
        logger.log(qtransport::LogLevel::info, "QPublicationTestDelegate::prepare");
        std::cerr << "pub prepare " << std::endl; 
        return 0;
    }
    int update(const std::string& sourceId, const std::string& qualityProfile) {
        logger.log(qtransport::LogLevel::info, "QPublicationTestDelegate::update");
        return 1; //1 = needs prepare
    }
    void publish(bool pubFlag) {
        logger.log(qtransport::LogLevel::info, "QPublicationTestDelegate::publish");
    }

private:
    quicr::Namespace quicrNamespace;
    qmedia::basicLogger logger;
};

class QPubisherTestDelegate : public qmedia::QPublisherDelegate
{
public:
    QPubisherTestDelegate()
    {
        logger.log(qtransport::LogLevel::info, "QPubisherTestDelegate constructed");
    }

    std::shared_ptr<qmedia::QPublicationDelegate> allocatePubByNamespace(const quicr::Namespace& quicrNamespace)
    {
       logger.log(qtransport::LogLevel::info, "QPubisherTestDelegate::allocatePubByNamespace");        
       std::cerr << "allocatePubByNamespace " << quicrNamespace.to_hex() << std::endl;
       return std::make_shared<QPublicationTestDelegate>(quicrNamespace);
    }

    int removePubByNamespace(const quicr::Namespace& quicrNamespace) 
    {
       logger.log(qtransport::LogLevel::info, "QPubisherTestDelegate::removeByNamespace"); 
       return 0;
    }

private:
    qmedia::basicLogger logger;
};

int test()
{
   //qmedia::basicLogger logger;    
    quicr::Namespace quicrNamespace;

    auto qSubscriber = std::make_shared<QSubsciberTestDelegate>();
    auto qPublisher = std::make_shared<QPubisherTestDelegate>();
    auto qController = std::make_shared<qmedia::QController>(qSubscriber, qPublisher);

    //logger.log(qtransport::LogLevel::info, "connecting to qController");
    //qController->connect("192.168.1.211", 33435, quicr::RelayInfo::Protocol::QUIC);
    then = timeSinceEpochMillisec();
    qController->connect("relay.us-west-2.quicr.ctgpoc.com", 33437, quicr::RelayInfo::Protocol::QUIC);
    std::uint64_t now = timeSinceEpochMillisec();
    std::cerr << "connect duration " << now - then << std::endl;

    std::ifstream f("/tmp/manifest.json");

    // load file into string
    std::stringstream strStream;
    strStream << f.rdbuf();
    auto manifest = strStream.str();
    
    then = timeSinceEpochMillisec();
    qController->updateManifest(manifest);
    now = timeSinceEpochMillisec();
    std::cerr << "update manifest  duration " << now - then << std::endl;
    std::uint8_t *data = new std::uint8_t[256];

    int num_buckets = 100;

    timeBuckets.resize(num_buckets);

    std::cerr << "PUBLISH DATE ------------------------------- " << std::endl;
    for (int i=0; i<100; i++)
    {
        ++i;
        then = timeSinceEpochMillisec();
        timeBuckets[i] = then;
        qController->publishNamedObjectTest(data, 256, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cerr << "PUBLISH DATE -------------------------------  END " << std::endl;
    std::uint64_t tear_then  = timeSinceEpochMillisec();
    delete[] data;
    qController = nullptr;
    std::uint64_t tear_now  = timeSinceEpochMillisec();
    std::cerr << "TEST() ---- finished......" << tear_now - tear_then << std::endl;
}

int main(int /*argc*/, char** /*arg*/)
{
   for (int i=0; i<100; ++i)
   {
    test();
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(120));
}

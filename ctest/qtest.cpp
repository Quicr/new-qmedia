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
    /*
    quicr::Namespace getNamespace() override {
        logger.log(qtransport::LogLevel::info, "QSubscriptionTestDelegate::getNamespace");
        return quicrNamespace;
    }*/

    int subscribedObject(quicr::bytes&& data) override {
        std::cerr << "subscribedObject " << std::endl;
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

    std::shared_ptr<qmedia::QSubscriptionDelegate> allocateSubByNamespace(const quicr::Namespace& quicrNamespace)
    {
       logger.log(qtransport::LogLevel::info, "QSubscriberTestDelegate::allocateSubByNamespace");        
       return std::make_shared<QSubscriptionTestDelegate>(quicrNamespace);
    }

    int removeSubByNamespace(const quicr::Namespace& quicrNamespace) 
    {
       logger.log(qtransport::LogLevel::info, "QSubscriberTestDelegate::removeByNamespace"); 
       return 0;
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
        return 0;
    }
    int update(const std::string& sourceId, const std::string& qualityProfile) {
        logger.log(qtransport::LogLevel::info, "QPublicationTestDelegate::update");
        return 1; //1 = needs prepare
    }
    void publish(bool pubFlag) {
        logger.log(qtransport::LogLevel::info, "QPublicationTestDelegate::publish");
    }
    /*
    quicr::Namespace getNamespace()  {
        logger.log(qtransport::LogLevel::info, "QPublicationTestDelegate::getNamespace");
        return quicrNamespace;
    }*/


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

int main(int /*argc*/, char** /*arg*/)
{
    //qmedia::basicLogger logger;    
    quicr::Namespace quicrNamespace;

    auto qSubscriber = std::make_shared<QSubsciberTestDelegate>();
    auto qPublisher = std::make_shared<QPubisherTestDelegate>();
    auto qController = std::make_shared<qmedia::QController>(qSubscriber, qPublisher);

    //logger.log(qtransport::LogLevel::info, "connecting to qController");
    //qController->connect("192.168.1.211", 33434, quicr::RelayInfo::Protocol::UDP);
    qController->connect("relay.us-west-2.quicr.ctgpoc.com", 33437, quicr::RelayInfo::Protocol::QUIC);

    std::ifstream f("/Users/shenning/M10x/WxQ.mani2/dependencies/new-qmedia/build/manifest.json");

    // load file into string
    std::stringstream strStream;
    strStream << f.rdbuf();
    auto manifest = strStream.str();
    
    qController->updateManifest(manifest);

    std::uint8_t *data = new std::uint8_t[256];

    while(true)
    {

       // qController->publishNamedObjectTest(data, 256);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

}

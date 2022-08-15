#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <QCoreApplication>

#include "Types/MissionConfig.hpp"

class BladeRfDeviceController;
class RawDataWriter;
class RawData;

class Application : public QCoreApplication
{
    Q_OBJECT
public:
    Application(int argc, char** argv);
    ~Application();

    void exit(int code = 0);

private slots:
    void onEventLoopStarted();

private slots:
    void onDeviceOpened();
    void onDeviceClosed();
    void onDeviceError();
    void onSessionStarted();
    void onSessionStopped();

private:
    BladeRfDeviceController* mDevice = nullptr;
    RawDataWriter* mWriter = nullptr;
    MissionConfig mConfig;

};

#endif // APPLICATION_HPP

#include <QThread>
#include <QTimer>
#include <QFile>

#include "BladeRfDeviceController.hpp"
#include "RawDataWriter.hpp"
#include "Application.hpp"

#define DeviceCall(device, command)                     QMetaObject::invokeMethod(device, &BladeRfDeviceController::command, Qt::QueuedConnection);
#define DeviceCallArgs(device, command, argsType, args) QMetaObject::invokeMethod(device, command, Qt::QueuedConnection, Q_ARG(argsType, args));

Application::Application(int argc, char** argv)
    : QCoreApplication(argc, argv)
{
    QTimer::singleShot(0, this, &Application::onEventLoopStarted);
}

Application::~Application()
{
    const auto deleteThreaded = [](QObject* obj) {
        if (!obj) return;
        const auto thread = obj->thread();

        thread->quit();
        thread->wait();
    };

    deleteThreaded(mDevice);
    deleteThreaded(mWriter);
}

void Application::exit(int code)
{
    DeviceCall(mDevice, sessionStop);
    QCoreApplication::exit(code);
}

void Application::onEventLoopStarted()
{
    bladerf_devinfo* deviceList = nullptr;
    QFile settingsFile("settings.json");

    if (!settingsFile.open(QIODevice::ReadOnly))
        qFatal("Can't open settings file: %s", qPrintable(settingsFile.errorString()));

    const auto fileContent = settingsFile.readAll();
    mConfig.raw(fileContent);

    if (!mConfig.valid())
        qFatal("Settings file invalid!");

    const auto count = bladerf_get_device_list(&deviceList);
    if (count < 0) qFatal("No bladeRF devices found!");
    else qInfo("%i bladeRF devices found. Using first...", count);

    const auto deviceInfo = deviceList[0];
    auto device = new BladeRfDeviceController;
    auto thread = new QThread(this);

    connect(thread, &QThread::finished,
            device, &BladeRfDeviceController::deleteLater);
    connect(device, &BladeRfDeviceController::opened,
            this,   &Application::onDeviceOpened,
            Qt::QueuedConnection);
    connect(device, &BladeRfDeviceController::closed,
            this,   &Application::onDeviceClosed,
            Qt::QueuedConnection);
    connect(device, &BladeRfDeviceController::errorOccured,
            this,   &Application::onDeviceError,
            Qt::QueuedConnection);
    connect(device, &BladeRfDeviceController::sessionStarted,
            this,   &Application::onSessionStarted,
            Qt::QueuedConnection);
    connect(device, &BladeRfDeviceController::sessionStopped,
            this,   &Application::onSessionStopped,
            Qt::QueuedConnection);

    if (mConfig.direction == Direction::RX)
    {
        const auto thread = new QThread(this);
        mWriter = new RawDataWriter;

        connect(thread,  &QThread::started,
                mWriter, &RawDataWriter::init);
        connect(thread,  &QThread::finished,
                mWriter, &RawDataWriter::deleteLater);

        connect(device,  &BladeRfDeviceController::rxDataAvailable,
                mWriter, &RawDataWriter::onData,
                Qt::QueuedConnection);

        mWriter->moveToThread(thread);
        thread->setObjectName("rx writer");
        thread->start();
    }

    device->moveToThread(thread);
    thread->setObjectName(deviceInfo.serial);
    thread->start();

    mDevice = device;

    DeviceCallArgs(device, "deviceOpen", bladerf_devinfo, deviceInfo);

    bladerf_free_device_list(deviceList);
}

void Application::onDeviceOpened()
{
    mDevice->printAboutDevice();
    mDevice->sessionStart(mConfig);
}

void Application::onDeviceClosed()
{
    qDebug("Device closed");
    exit(0);
}

void Application::onDeviceError()
{
    exit(-1);
}

void Application::onSessionStarted()
{
    qDebug("Session started");
}

void Application::onSessionStopped()
{
    qDebug("Session stopped");
}

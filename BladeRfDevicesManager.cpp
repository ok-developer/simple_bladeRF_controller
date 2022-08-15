#include <QThread>

#include "BladeRfDeviceController.hpp"
#include "BladeRfDevicesManager.hpp"

#define DetermineDeviceFromCall                         const auto device = qobject_cast<BladeRfDeviceController*>(sender());
#define DeviceCall(device, command)                     QMetaObject::invokeMethod(device, &BladeRfDeviceController::command, Qt::QueuedConnection);
#define DeviceCallArgs(device, command, argsType, args) QMetaObject::invokeMethod(device, command, Qt::QueuedConnection, Q_ARG(argsType, args));

BladeRfDevicesManager::BladeRfDevicesManager(QObject* parent)
    : QObject(parent)
{

}

BladeRfDevicesManager::~BladeRfDevicesManager()
{
    for (auto device : qAsConst(mDevices))
    {
        device.handle->sessionStop();
        device.handle->thread()->quit();
        device.handle->thread()->wait();
    }
}

QList<BladeRfDeviceController*> BladeRfDevicesManager::devices() const
{
    QList<BladeRfDeviceController*> result;
    for (const auto& holder : mDevices)
        result.append(holder.handle);
    return result;
}

BladeRfDeviceController* BladeRfDevicesManager::device(SdrDeviceType type) const
{
    for (const auto& holder : mDevices)
        if (holder.handle->sdrDeviceType() == type) return holder.handle;
    return nullptr;
}

void BladeRfDevicesManager::init()
{
    const auto buffer = Application()->buffer();
    bladerf_devinfo* deviceList = nullptr;

    mDevicesCount = bladerf_get_device_list(&deviceList);
    if (mDevicesCount < 0)
    {
        qWarning("No bladeRF devices found!");
        return;
    }
    else qDebug("%i devices found", mDevicesCount);

    for (int i = 0; i < mDevicesCount; ++i)
    {
        auto device = new BladeRfDeviceController;
        auto thread = new QThread(this);

        connect(thread, &QThread::finished,
                device, &BladeRfDeviceController::deleteLater);
        connect(device, &BladeRfDeviceController::opened,
                this,   &BladeRfDevicesManager::onDeviceOpened,
                Qt::QueuedConnection);
        connect(device, &BladeRfDeviceController::closed,
                this,   &BladeRfDevicesManager::onDeviceClosed,
                Qt::QueuedConnection);
        connect(device, &BladeRfDeviceController::errorOccured,
                this,   &BladeRfDevicesManager::onDeviceError,
                Qt::QueuedConnection);
        connect(device, &BladeRfDeviceController::sessionStarted,
                this,   &BladeRfDevicesManager::onCaptureStarted,
                Qt::QueuedConnection);
        connect(device, &BladeRfDeviceController::sessionStopped,
                this,   &BladeRfDevicesManager::onCaptureStopped,
                Qt::QueuedConnection);

        connect(device,       &BladeRfDeviceController::rxDataAvailable,
                buffer.get(), &RxSignalDataBuffer::onCaptureAvailable,
                Qt::QueuedConnection);
        connect(device,       &BladeRfDeviceController::sessionStarted,
                buffer.get(), &RxSignalDataBuffer::onSessionStateChanged,
                Qt::QueuedConnection);

        device->moveToThread(thread);
        thread->setObjectName(deviceList[i].serial);
        thread->start();

        DeviceCallArgs(device, "deviceOpen", bladerf_devinfo, deviceList[i]);
    }

    bladerf_free_device_list(deviceList);
}

void BladeRfDevicesManager::startCapture(const SessionRadioSettings& config)
{
    for (const auto& device : qAsConst(mDevices))
        DeviceCallArgs(device.handle, "sessionStart", SessionRadioSettings, config);
}

void BladeRfDevicesManager::stopCapture()
{
    for (const auto& device : qAsConst(mDevices))
        DeviceCall(device.handle, sessionStop);
}

void BladeRfDevicesManager::onDeviceOpened()
{
    DetermineDeviceFromCall;
    const auto type = device->sdrDeviceType();

    device->printAboutDevice();

    mDevices.insert(type, Device(device));
    device->thread()->setObjectName(sdrDeviceTypeToUiTag(type));

    if (device->sdrDeviceType() == SdrDeviceType::Unknown)
    {
        qWarning("Unknown device: %s", qPrintable(device->sdrDeviceSerial()));
        emit errorOccured();
    }
    else if (mDevices.count() == mDevicesCount)
    {
        if (auto handle = mDevices[SdrDeviceType::FirstDirection].handle; handle)
        {
            handle->enableClockBroadcast();

            if (handle = mDevices[SdrDeviceType::SecondDirection].handle; handle)
            {
                handle->enableClockListening();
                handle->enableClockBroadcast();

                if (handle = mDevices[SdrDeviceType::ThirdDirection].handle; handle)
                {
                    handle->enableClockListening();

                    emit initialized();
                }
            }
        }
    }
}

void BladeRfDevicesManager::onDeviceClosed()
{
    DetermineDeviceFromCall;
    mDevices.insert(device->sdrDeviceType(), Device(device));

    if (mDevices.count() == 0) emit deinitialized();
}

void BladeRfDevicesManager::onDeviceError()
{
    emit errorOccured();
}

void BladeRfDevicesManager::onCaptureStarted()
{
    DetermineDeviceFromCall;
    mDevices[device->sdrDeviceType()].captureState = true;

    for (const auto& device : qAsConst(mDevices))
        if (not device.captureState) return;

    std::this_thread::sleep_for(std::chrono::seconds(1));   // needs to fire after all streams starts and must be in this thread to block
    qDebug("Trigger fire");
    mDevices[SdrDeviceType::FirstDirection].handle->triggerFire();

    emit started();
}

void BladeRfDevicesManager::onCaptureStopped()
{
    DetermineDeviceFromCall;
    mDevices[device->sdrDeviceType()].captureState = false;

    for (const auto& device : qAsConst(mDevices))
        if (device.captureState) return;
    emit stopped();
}

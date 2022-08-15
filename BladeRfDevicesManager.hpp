#pragma once

#include <QObject>
#include <QMap>

class BladeRfDeviceController;

class BladeRfDevicesManager : public QObject
{
    Q_OBJECT

    struct Device
    {
        Device() = default;
        Device(BladeRfDeviceController* handle)
            : handle(handle) {}

        BladeRfDeviceController* handle = nullptr;
        bool captureState = false;
    };

signals:
    void initialized();
    void deinitialized();
    void errorOccured();

    void started();
    void stopped();

public:
    explicit BladeRfDevicesManager(QObject* parent = nullptr);
    ~BladeRfDevicesManager();

    QList<BladeRfDeviceController*> devices() const;
    BladeRfDeviceController* device(SdrDeviceType type) const;

public slots:
    void init();

    void startCapture(const SessionRadioSettings& config);
    void stopCapture();

private slots:
    void onDeviceOpened();
    void onDeviceClosed();
    void onDeviceError();
    void onCaptureStarted();
    void onCaptureStopped();

private:
    QMap<SdrDeviceType, Device> mDevices;

    int mDevicesCount = 0;
};


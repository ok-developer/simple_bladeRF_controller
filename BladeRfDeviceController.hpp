#pragma once

#include <QObject>

#include <libbladeRF.h>

#include "Types/MissionConfig.hpp"

#define BladeRFUndefined                    -1
#ifndef MIXING_SIGNAL
    #define MIXING_SIGNAL                   true
    #define MIXING_SIGNAL_SINGLE            MIXING_SIGNAL and true
    #define MIXING_SIGNAL_STREAM            MIXING_SIGNAL and false
    #if MIXING_SIGNAL_STREAM
        #define MIXING_SIGNAL_TEST          false
        #define MIXING_SIGNAL_ADVANCED      false
    #endif
#endif

class BladeRfStream;
class RawData;

class BladeRfDeviceController : public QObject
{
    Q_OBJECT
signals:
    void opened();
    void closed();
    void errorOccured();
    void sessionStarted();
    void sessionStopped();
    void rxDataAvailable(const RawData& data);

public:
    explicit BladeRfDeviceController(QObject* parent = nullptr);
    ~BladeRfDeviceController();

    void enableClockBroadcast();
    void enableClockListening();
    void setReferenceClock();

    bool triggerFire();
    void printAboutDevice();

public slots:
    void deviceOpen(const bladerf_devinfo deviceInfo);
    void deviceClose();

    void sessionStart(const MissionConfig& config);
    void sessionStop();

private:
    bool error(const QString& message, int bladerf_error = 0, bladerf_channel module = BladeRFUndefined);
    void log(const QString& message, bladerf_channel module = BladeRFUndefined) const;

    QString moduleToString(bladerf_channel module) const;

    bool moduleSetup(bladerf_direction direction, bladerf_module module);
    bool moduleState(bladerf_module module, bool state);
    bool moduleGain(bladerf_module module, int value);

    void deviceSilentReopen();

private slots:
    void onRxCaptureAvailable(qint16* buffer, unsigned short samplesCount);

private:
    MissionConfig mSessionConfig;

    bladerf_devinfo mDeviceInfo;
    bladerf* mDeviceHandle = nullptr;

    std::atomic_bool mCaptureProcessFlag;

    BladeRfStream* mRxStream = nullptr;
    BladeRfStream* mTxStream = nullptr;
};

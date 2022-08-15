#include <QDir>

#include "Types/RawData.hpp"

#include "BladeRfDeviceController.hpp"
#include "BladeRfStream.hpp"

#define PrintError(message, function)   { int status = function; if (status not_eq 0) { return error(message, status); }
#define PrintErrorV(message, function)  { int status = function; if (status not_eq 0) { error(message, status); return; } }

#define RX1                             BLADERF_CHANNEL_RX(0)
#define RX2                             BLADERF_CHANNEL_RX(1)
#define TX1                             BLADERF_CHANNEL_TX(0)
#define TX2                             BLADERF_CHANNEL_TX(1)

#define RX_BUFFERS_COUNT                32
#define TX_BUFFERS_COUNT                16
#define RX_CHANNELS_COUNT               2
#define BLADERF_FOLDER_NAME             "bladeRF"
#define FPGA_FOLDER_NAME                "fpga"
#define FPGA_FOLDER_PATH                QDir::currentPath() + '/' + BLADERF_FOLDER_NAME + '/' + FPGA_FOLDER_NAME

inline bool printError(const QString& message, int status)
{
    if (status not_eq 0)
    {
        qWarning("Error: %s | %s", qPrintable(message), bladerf_strerror(status));
        return false;
    }
    return true;
}

BladeRfDeviceController::BladeRfDeviceController(QObject* parent)
    : QObject(parent),
      mCaptureProcessFlag(false)
{
    qRegisterMetaType<bladerf_devinfo>("bladerf_devinfo");
    qRegisterMetaType<RawData>("RawData");
    qRegisterMetaType<short*>("short*");
}

BladeRfDeviceController::~BladeRfDeviceController()
{
    sessionStop();
    if (mTxStream) delete mTxStream;
    if (mRxStream) delete mRxStream;
    deviceClose();
}

void BladeRfDeviceController::enableClockBroadcast()
{
    // Иногда, почему-то, появляется ошибка записи в gpio. Такой вот костыль
    while (true)
    {
        int status = bladerf_set_clock_output(mDeviceHandle, true);
        if (status == 0) break;

        log(QString("Failed to set external clock output: %1. Trying again...").arg(bladerf_strerror(status)));
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    log("Clock broadcast");
}

void BladeRfDeviceController::enableClockListening()
{
    // Иногда, почему-то, появляется ошибка записи в gpio. Такой вот костыль
    while (true)
    {
        const auto status = bladerf_set_clock_select(mDeviceHandle, bladerf_clock_select::CLOCK_SELECT_EXTERNAL);
        if (status == 0) break;

        log(QString("Failed to set external clock input: %1. Trying again...").arg(bladerf_strerror(status)));

        blockSignals(true);
        deviceClose();
        deviceOpen(mDeviceInfo);
        blockSignals(false);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    log("Clock listening");
}

void BladeRfDeviceController::setReferenceClock()
{
    int status = bladerf_set_clock_select(mDeviceHandle, bladerf_clock_select::CLOCK_SELECT_ONBOARD);
    if (status not_eq 0)
    {
        error("Failed to set onboard clock", status);
        return;
    }

    status = bladerf_set_pll_enable(mDeviceHandle, true);
    if (status not_eq 0)
    {
        error("Failed to enable pll", status);
        return;
    }

    status = bladerf_set_pll_refclk(mDeviceHandle, 10e6);
    if (status not_eq 0)
    {
        error("Failed to set pll frequency", status);
        return;
    }
}

bool BladeRfDeviceController::triggerFire()
{
    return printError("rx trigger fire", mRxStream->triggerFire());
}

void BladeRfDeviceController::deviceOpen(const bladerf_devinfo deviceInfo)
{
    int status = 0;

    mDeviceInfo = deviceInfo;
    memcpy(mDeviceInfo.manufacturer, deviceInfo.manufacturer, strlen(deviceInfo.manufacturer));
    memcpy(mDeviceInfo.product, deviceInfo.product, strlen(deviceInfo.product));
    memcpy(mDeviceInfo.serial, deviceInfo.serial, strlen(deviceInfo.serial));

    {   // device handle open
        for (int i = 0; i < 3; ++i)
        {
            status = bladerf_open_with_devinfo(&mDeviceHandle, &mDeviceInfo);
            if (status == 0) break;
        }
        if (!mDeviceHandle)
        {
            error("Failed to open device", status);
            return;
        }
    }

    {   // FPGA load
        if (status = bladerf_is_fpga_configured(mDeviceHandle); status != 1)
        {
            const QDir fpgaDir(FPGA_FOLDER_PATH);
            const auto fpgaFiles = fpgaDir.entryList(QDir::Files);
            size_t size = 0;

            if (status = bladerf_get_fpga_bytes(mDeviceHandle, &size); status < 0)
            {
                error("Failed to determine FPGA image size", status);
                deviceClose();
                return;
            }

            for (const auto& entry : fpgaFiles)
            {
                const auto entryPath = fpgaDir.absoluteFilePath(entry);

                if (QFileInfo(entryPath).size() == size)
                {
                    if (status = bladerf_load_fpga(mDeviceHandle, entryPath.toUtf8()); status < 0)
                    {
                        error("Failed to load FPGA image", status);
                        deviceClose();
                        return;
                    }
                    else
                    {
                        log(QString("%1 fpga loaded").arg(entry));
                        break;
                    }
                }
            }

            status = bladerf_is_fpga_configured(mDeviceHandle);
            if (status < 0)
            {
                error("Failed to determine FPGA state", status);
                deviceClose();
                return;
            }
            else if (status == 0)
            {
                error("FPGA not loaded");
                deviceClose();
                return;
            }
        }
        else qDebug("FPGA loaded");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    //setReferenceClock();

    emit opened();
}

void BladeRfDeviceController::sessionStart(const MissionConfig& config)
{
    BladeRfStream* stream = nullptr;
    bladerf_channel_layout layout = bladerf_channel_layout::BLADERF_RX_X1;
    bladerf_direction direction = bladerf_direction::BLADERF_RX;
    bladerf_channel channel = 0;

    mSessionConfig = config;

    switch (config.direction)
    {
        case Direction::RX:
        {
            stream = mRxStream = new BladeRfStream(mDeviceHandle, BLADERF_RX, RX_BUFFERS_COUNT);

            connect(mRxStream, &BladeRfStream::data,
                    this,      &BladeRfDeviceController::onRxCaptureAvailable,
                    Qt::QueuedConnection);
            connect(mRxStream, &BladeRfStream::errorOccured,
                    this,      &BladeRfDeviceController::errorOccured,
                    Qt::QueuedConnection);

            direction = BLADERF_RX;
            layout = BLADERF_RX_X2;

            if (!moduleSetup(direction, RX1)
            ||  !moduleSetup(direction, RX2)
            ||  !moduleState(RX1, true)
            ||  !moduleState(RX2, true)
            ||  !moduleGain(RX1, config.gain)
            ||  !moduleGain(RX2, config.gain))
            {
                sessionStop();
                return;
            }
        }
        break;
        case Direction::TX:
        {
            stream = mTxStream = new BladeRfStream(mDeviceHandle, BLADERF_TX, TX_BUFFERS_COUNT);

            connect(mTxStream, &BladeRfStream::errorOccured,
                    this,      &BladeRfDeviceController::errorOccured,
                    Qt::QueuedConnection);

            direction = BLADERF_TX;
            layout = BLADERF_TX_X1;
            channel = config.channel == Channel::One ? TX1 : TX2;

            if (!moduleSetup(direction, channel)
            ||  !moduleState(channel, true)
            ||  !moduleGain(channel, config.gain))
            {
                sessionStop();
                return;
            }
        }
        break;
    }

    PrintErrorV("stream init", stream->streamInit(config));
    PrintErrorV("stream start", stream->streamStart(layout));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    mCaptureProcessFlag.store(true);
    emit sessionStarted();
}

void BladeRfDeviceController::sessionStop()
{
    if (mTxStream)
    {
        PrintErrorV("tx stream stop", mTxStream->streamStop());
        PrintErrorV("tx trigger disarm", mTxStream->triggerDisarm());
        PrintErrorV("tx stream deinit", mTxStream->streamDeinit());
    }
    if (mRxStream)
    {
        PrintErrorV("rx stream stop", mRxStream->streamStop());
        PrintErrorV("rx trigger disarm", mRxStream->triggerDisarm());
        PrintErrorV("rx stream deinit", mRxStream->streamDeinit());
    }

    mCaptureProcessFlag.store(false);
    emit sessionStopped();
}

bool BladeRfDeviceController::error(const QString& message, int bladerf_error, bladerf_channel module)
{
    if (module != BladeRFUndefined)
        qWarning("[%s] %s: %s",
                 qPrintable(moduleToString(module)),
                 qPrintable(message),
                 bladerf_strerror(bladerf_error));
    else
        qWarning("%s: %s",
                 qPrintable(message),
                 bladerf_strerror(bladerf_error));

    emit errorOccured();

    if (mCaptureProcessFlag.load())
        sessionStop();

    return false;
}

void BladeRfDeviceController::log(const QString& message, bladerf_channel module) const
{
    if (module != BladeRFUndefined)
        qDebug("[%s] %s.",
               qPrintable(moduleToString(module)),
               qPrintable(message));
    else
        qDebug("%s.",
               qPrintable(message));
}

QString BladeRfDeviceController::moduleToString(bladerf_channel module) const
{
    switch (module)
    {
    case BLADERF_CHANNEL_RX(0): return "RX1";
    case BLADERF_CHANNEL_RX(1): return "RX2";
    case BLADERF_CHANNEL_TX(0): return "TX1";
    case BLADERF_CHANNEL_TX(1): return "TX2";
    default: return "Undefined";
    }
}

void BladeRfDeviceController::deviceClose()
{
    if (mDeviceHandle)
    {
        bladerf_close(mDeviceHandle);
        mDeviceHandle = nullptr;
        emit closed();
    }
}

void BladeRfDeviceController::printAboutDevice()
{
    qDebug("Device information:"
           "\n  Serial: %s",
           mDeviceInfo.serial);
}

bool BladeRfDeviceController::moduleSetup(bladerf_direction direction, bladerf_module module)
{
    const auto onError = [&](const QString& errorString, int status) -> bool {
        error(errorString, status, module);
        return false;
    };

    int status = bladerf_set_frequency(mDeviceHandle, module, mSessionConfig.frequency);
    if (status not_eq 0) return onError("Failed to set frequency", status);
    else
    {
        bladerf_frequency frequency = 0;
        bladerf_get_frequency(mDeviceHandle, module, &frequency);
        log("Frequency set to " + QString::number(frequency), module);
    }

    status = bladerf_set_sample_rate(mDeviceHandle, module, mSessionConfig.sampleRate, nullptr);
    if (status not_eq 0) return onError("Failed to set samplerate", status);
    else
    {
        bladerf_sample_rate samplerate = 0;
        bladerf_get_sample_rate(mDeviceHandle, module, &samplerate);
        log("Samplerate set to " + QString::number(samplerate), module);
    }

    status = bladerf_set_bandwidth(mDeviceHandle, module, mSessionConfig.bandwidth, nullptr);
    if (status not_eq 0) return onError("Failed to set bandwidth", status);
    else
    {
        bladerf_bandwidth bandwidth = 0;
        bladerf_get_bandwidth(mDeviceHandle, module, &bandwidth);
        log("Bandwidth set to " + QString::number(bandwidth), module);
    }

    status = bladerf_set_rfic_rx_fir(mDeviceHandle, bladerf_rfic_rxfir::BLADERF_RFIC_RXFIR_DEC4);
    if (status not_eq 0) return onError("Failed to set rx rfic", status);

    status = bladerf_set_rfic_tx_fir(mDeviceHandle, bladerf_rfic_txfir::BLADERF_RFIC_TXFIR_INT4);
    if (status not_eq 0) return onError("Failed to set tx rfic", status);

    status = bladerf_set_gain_mode(mDeviceHandle, module, bladerf_gain_mode::BLADERF_GAIN_MGC);
    if (status not_eq 0) return onError("Failed to set manual gain mode", status);

    //status = bladerf_set_bias_tee(mDeviceHandle, module, true);
    //if (status not_eq 0) return onError("Failed to set bias tee", status);

    log("Setup completed", module);
    return true;
}

bool BladeRfDeviceController::moduleState(bladerf_module module, bool state)
{
    const int status = bladerf_enable_module(mDeviceHandle, module, state);
    const QString state_str = state ? "true" : "false";
    if (status < 0)
    {
        error("Failed to set module state to " + state_str, status, module);
        return false;
    }
    log("Module set state to " + state_str, module);

    return true;
}

bool BladeRfDeviceController::moduleGain(bladerf_module module, int value)
{
    const int status = bladerf_set_gain(mDeviceHandle, module, value);
    if (status != 0)
    {
        error("Failed to set gain", status, module);
        return false;
    }
    //log("Gain set to " + QString::number(value), module);
    return true;
}

void BladeRfDeviceController::deviceSilentReopen()
{
    blockSignals(true);
    deviceClose();
    deviceOpen(mDeviceInfo);
    blockSignals(false);
}

void BladeRfDeviceController::onRxCaptureAvailable(qint16* buffer, unsigned short samplesCount)
{
    if (!mCaptureProcessFlag.load()) return; // In the end of capture session
                                             //   it can enter here and crash. Why?

    bladerf_deinterleave_stream_buffer(BLADERF_RX_X2,
                                       BLADERF_FORMAT_SC16_Q11,
                                       samplesCount,
                                       buffer);

    emit rxDataAvailable(RawData(buffer, mSessionConfig.samplesCount));
}

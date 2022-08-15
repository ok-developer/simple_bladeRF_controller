#include <QFile>
#include <QDir>

#include <complex>
#include <cmath>

#include "Types/RawData.hpp"

#include "BladeRfStream.hpp"

#define ExecStatus(function)    { int status = function; if (status not_eq 0) { return status; } }

#define BLADERF_DAC_MAX         1023.0

BladeRfStream::BladeRfStream(bladerf* deviceHandle, bladerf_direction direction, ushort buffersCount)
    : deviceHandle(deviceHandle),
      mutex(new std::mutex),
      trigger(new bladerf_trigger),
      triggerRole(bladerf_trigger_role::BLADERF_TRIGGER_ROLE_DISABLED),
      direction(direction),
      bufferIterator(0),
      buffersCount(buffersCount),
      processFlag(false)
{

}

BladeRfStream::~BladeRfStream()
{
    if (streamThread)
    {
        processFlag.store(false);
        if (streamThread->joinable())
            streamThread->join();
        delete streamThread;
    }

    triggerDisarm();
    if (trigger) delete trigger;

    if (stream) bladerf_deinit_stream(stream);
    if (mutex) delete mutex;
    if (buffers)
    {
        for (ushort i = 0; i < buffersCount; ++i)
            if (buffers[i])
                delete[] buffers[i];
        delete buffers;
    }
}

int BladeRfStream::triggerArm(bladerf_trigger_role role)
{
    ExecStatus(bladerf_trigger_init(deviceHandle, direction, BLADERF_TRIGGER_J51_1, trigger));
    trigger->role = triggerRole = role;
    ExecStatus(bladerf_trigger_arm(deviceHandle, trigger, true, 0, 0));
    return 0;
}

int BladeRfStream::triggerDisarm()
{
    if (triggerRole not_eq bladerf_trigger_role::BLADERF_TRIGGER_ROLE_DISABLED)
    {
        trigger->role = triggerRole = BLADERF_TRIGGER_ROLE_DISABLED;
        return bladerf_trigger_arm(deviceHandle, trigger, false, 0, 0);
    }

    return 0;
}

int BladeRfStream::triggerRearm()
{
    const auto lastRole = triggerRole;
    ExecStatus(triggerDisarm());
    return triggerArm(lastRole);
}

int BladeRfStream::triggerFire()
{
    return bladerf_trigger_fire(deviceHandle, trigger);
}

int BladeRfStream::streamInit(const MissionConfig& config)
{
    const auto transfersCount = (buffersCount > 1) ? buffersCount / 2 : 1;

    this->config = config;

    return bladerf_init_stream(&stream,
                               deviceHandle,
                               &BladeRfStream::callback,
                               reinterpret_cast<void***>(&buffers),
                               buffersCount,
                               BLADERF_FORMAT_SC16_Q11,
                               config.samplesCount,
                               transfersCount,
                               this);
}

int BladeRfStream::streamDeinit()
{
    bladerf_deinit_stream(stream);
    stream = nullptr;
    buffers = nullptr;
    return 0;
}

int BladeRfStream::streamStart(bladerf_channel_layout layout)
{
    streamStop();

    if (direction == BLADERF_TX) generateTxBuffers();

    streamThread = new std::thread([this, layout]()
    {
        processFlag.store(true);
        const auto status = bladerf_stream(stream, layout);
        processFlag.store(false);

        if (status not_eq 0)
        {
            qWarning("stream error: %s", bladerf_strerror(status));
            emit errorOccured();
        }
    });

    return 0;
}

int BladeRfStream::streamStop()
{
    if (streamThread)
    {
        processFlag.store(false);
        if (streamThread->joinable())
            streamThread->join();
        delete streamThread;
        streamThread = nullptr;
    }

    return 0;
}

void BladeRfStream::generateTxBuffers()
{
    // TODO: read from file
    QFile txData(QDir::current().absoluteFilePath(config.fileName));

    if (!txData.open(QIODevice::ReadOnly))
        qFatal("Can't open tx data file: %s", qPrintable(txData.errorString()));

    const auto data = txData.read(config.samplesCount * SAMPLE_SIZE_BYTES);
    const auto dataPtr = (short*)data.data();

    buffers = new short*[buffersCount];

    for (quint32 buffer = 0; buffer < buffersCount; ++buffer)
    {
        const auto samplesCount = config.samplesCount;

        buffers[buffer] = new short[samplesCount];
        for (quint64 i = 0; i < samplesCount; i++)
        {
            buffers[buffer][i] = dataPtr[i];
            buffers[buffer][i + 1] = dataPtr[i + 1];
        }
    }
}

void* BladeRfStream::callback(bladerf*, struct bladerf_stream*, bladerf_metadata*,
                              void* data, size_t samplesCount, void* deviceInstance)
{
    auto instance = static_cast<BladeRfStream*>(deviceInstance);
    if (!instance) return BLADERF_STREAM_SHUTDOWN;

    if (!instance->processFlag.load()) return BLADERF_STREAM_SHUTDOWN;
    else if (instance->direction == BLADERF_RX)
        emit instance->data(reinterpret_cast<short*>(data), samplesCount);

    if (++instance->bufferIterator >= instance->buffersCount)
        instance->bufferIterator.store(0);
    return instance->buffers[instance->bufferIterator.load()];
}

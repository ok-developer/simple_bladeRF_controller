#pragma once

#include <QObject>

#include <atomic>
#include <thread>
#include <mutex>

#include <libbladeRF.h>

#include "Types/MissionConfig.hpp"

struct BladeRfStream : public QObject
{
    Q_OBJECT
signals:
    void errorOccured();
    void data(short* buffer, short samplesCount);

public:
    BladeRfStream() = default;
    BladeRfStream(bladerf* deviceHandle, bladerf_direction direction, ushort buffersCount);
    ~BladeRfStream();

    int triggerArm(bladerf_trigger_role role);
    int triggerDisarm();
    int triggerRearm();
    int triggerFire();

    int streamInit(const MissionConfig& config);
    int streamDeinit();
    int streamStart(bladerf_channel_layout layout);
    int streamStop();

private:
    void generateTxBuffers();

private:
    static void* callback(bladerf* device, struct bladerf_stream* stream, bladerf_metadata* meta,
                          void* samples, size_t samplesCount, void* deviceInstance);

private:
    bladerf* deviceHandle = nullptr;
    std::thread* streamThread = nullptr;
    struct bladerf_stream* stream = nullptr;
    mutable std::mutex* mutex = nullptr;
    bladerf_trigger* trigger = nullptr;
    short** buffers = nullptr;
    bladerf_trigger_role triggerRole;
    bladerf_direction direction;
    std::atomic_uint16_t bufferIterator;
    std::atomic_uint16_t buffersCount;
    std::atomic_bool processFlag;

    MissionConfig config;

};

#pragma once

#include "JsonConfig.hpp"

#define UNLIMITED 0

class QStringList;

enum class Direction
{
    RX = 1,
    TX
};

enum class Channel
{
    One = 1,
    Two
};

class MissionConfig : public JsonConfig
{
public:
    ~MissionConfig() = default;

    virtual bool valid() const override
    {
        return samplesCount != 0
            && sampleRate != 0
            && frequency != 0
            && bandwidth != 0;
    };

    virtual void fromJson(const QJsonObject& json) override;
    virtual void fillJson(QJsonObject& json) const override;

public:
    unsigned long long samplesCount = 0;
    unsigned long long sampleRate = 0;
    unsigned long long frequency = 0;
    unsigned long long bandwidth = 0;
    Direction direction = Direction::RX;
    Channel channel = Channel::One;
    unsigned tryCount = UNLIMITED;
    unsigned short gain = 0;

    QString fileName;
};

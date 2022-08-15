#include "MissionConfig.hpp"

DefineJsonField(samples_count)
DefineJsonField(samplerate)
DefineJsonField(file_name)
DefineJsonField(frequency)
DefineJsonField(bandwidth)
DefineJsonField(direction)
DefineJsonField(channel)
DefineJsonField(tryes)
DefineJsonField(gain)

void MissionConfig::fromJson(const QJsonObject& json)
{
    samplesCount = json[i_samples_count].toString().toULongLong();
    sampleRate = json[i_samplerate].toString().toULongLong();
    frequency = json[i_frequency].toString().toULongLong();
    bandwidth = json[i_bandwidth].toString().toULongLong();
    direction = Direction(json[i_direction].toInt());
    channel = Channel(json[i_channel].toInt());
    tryCount = json[i_tryes].toInt();
    gain = json[i_gain].toInt();
    fileName = json[i_file_name].toString();
}

void MissionConfig::fillJson(QJsonObject& json) const
{

}

#include <QElapsedTimer>

#include <cstring>

#include "RawData.hpp"

RawData::RawData(const QByteArray& rx1, const QByteArray& rx2)
    : mRx1(rx1),
      mRx2(rx2)
{
    if (rx1.size() == rx2.size())
        mRxSizeBytes = rx1.size();
}

RawData::RawData(unsigned int samplesCount)
{
    mRxSizeBytes = samplesCount * SAMPLE_SIZE_BYTES;
    mRx1.resize(mRxSizeBytes);
    mRx2.resize(mRxSizeBytes);
}

RawData::RawData(const short* data, unsigned int samplesCount)
{
    const quint32 rx1Start = TRASH_SAMPLES_COUNT * sizeof(short);
    const quint32 rx2Start = (samplesCount + TRASH_SAMPLES_COUNT) * sizeof(short);
    mRxSizeBytes = (samplesCount - TRASH_SAMPLES_COUNT) * SAMPLE_SIZE_BYTES;

    mRx1.resize(mRxSizeBytes);
    mRx2.resize(mRxSizeBytes);

    std::memcpy(mRx1.data(), reinterpret_cast<const char*>(&data[rx1Start]), mRxSizeBytes);
    std::memcpy(mRx2.data(), reinterpret_cast<const char*>(&data[rx2Start]), mRxSizeBytes);
}

bool RawData::operator==(const RawData& other) const
{
    return mIndex == other.mIndex;
}

void RawData::setRX1(const QByteArray& rx1)
{
    mRx1 = rx1;
    if (mRxSizeBytes == 0)
    {
        mRxSizeBytes = rx1.size();
    }
}

void RawData::setRX2(const QByteArray& rx2)
{
    mRx2 = rx2;
    if (mRxSizeBytes == 0)
    {
        mRxSizeBytes = rx2.size();
    }
}

void RawData::setIndex(unsigned int index)
{
    mIndex = index;
}

void RawData::clear()
{
    mRx1.clear();
    mRx2.clear();
    mRxSizeBytes = 0;
}

bool RawData::valid() const
{
    return mRxSizeBytes not_eq 0;
}

quint32 RawData::rxSize() const
{
    return mRxSizeBytes;
}

unsigned int RawData::samplesCount() const
{
    return mRxSizeBytes / SAMPLE_SIZE_BYTES;
}

unsigned int RawData::index() const
{
    return mIndex;
}

QByteArray RawData::rx1() const
{
    return mRx1;
}

QByteArray RawData::rx2() const
{
    return mRx2;
}

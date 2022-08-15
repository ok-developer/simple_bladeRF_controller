#pragma once

#include <QByteArray>

// | I(2-byte) | Q(2-byte) |
inline const quint8  SAMPLE_SIZE_BYTES                            = 2 * sizeof(qint16);
inline const quint32 TRASH_SAMPLES_COUNT                          = 0;    // 9728 Только если прерывистый захват

struct RawData
{
public:
    RawData() = default;
    RawData(unsigned int samplesCount);
    RawData(const QByteArray& rx1, const QByteArray& rx2);
    RawData(const short* data, unsigned int samplesCount);

    bool operator==(const RawData& other) const;

    void setRX1(const QByteArray& rx1);
    void setRX2(const QByteArray& rx2);
    void setIndex(unsigned int index);
    void clear();

    bool valid() const;
    /// Размер одного канала в байтах
    unsigned int rxSize() const;
    unsigned int samplesCount() const;
    unsigned int index() const;
    QByteArray rx1() const;
    QByteArray rx2() const;

public:
    QByteArray mRx1;
    QByteArray mRx2;

    unsigned int mIndex = 0;
    unsigned int mRxSizeBytes = 0;

};
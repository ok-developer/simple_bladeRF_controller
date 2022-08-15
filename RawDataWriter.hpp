#ifndef RAWDATAWRITER_HPP
#define RAWDATAWRITER_HPP

#include <QObject>

class QFile;
class RawData;

class RawDataWriter : public QObject
{
    Q_OBJECT
public:
    RawDataWriter(QObject* parent = nullptr);

public slots:
    void init();
    void onData(const RawData& data);

private:
    QFile* mRx1 = nullptr;
    QFile* mRx2 = nullptr;
};

#endif // RAWDATAWRITER_HPP

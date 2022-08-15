#include <QFile>
#include <QDir>

#include "Types/RawData.hpp"

#include "RawDataWriter.hpp"

RawDataWriter::RawDataWriter(QObject* parent)
    : QObject(parent)
{

}

void RawDataWriter::init()
{
    const auto openFile = [this](QFile*& file, int index) {
        const QString path(QDir::current().absoluteFilePath("rx%1.bin"));
        if (file) file->deleteLater();
        file = new QFile(path.arg(index), this);

        if (!file->open(QIODevice::WriteOnly))
            qFatal("Can't open file %s for write: %s",
                   qPrintable(file->fileName()),
                   qPrintable(file->errorString()));
    };

    openFile(mRx1, 1);
    openFile(mRx2, 2);
}

void RawDataWriter::onData(const RawData& data)
{
    const auto write = [](QFile* file, const QByteArray& data) {
        if (file && !data.isEmpty()) file->write(data);
    };

    write(mRx1, data.rx1());
    write(mRx2, data.rx2());
}

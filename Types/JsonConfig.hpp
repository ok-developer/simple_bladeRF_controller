#ifndef JSONCONFIG_HPP
#define JSONCONFIG_HPP

#include <QJsonDocument>
#include <QJsonObject>

#define DefineJsonField(name) \
    inline const char* i_##name = #name;

class JsonConfig
{
public:
    virtual ~JsonConfig() = default;

    virtual void fromJson(const QJsonObject& json) = 0;
    virtual void fillJson(QJsonObject& json) const = 0;
    inline virtual bool valid() const { return true; }

    inline QJsonObject json() const
    {
        QJsonObject json;
        fillJson(json);
        return json;
    }

    inline void raw(const QByteArray& data) { fromJson(QJsonDocument::fromJson(data).object()); }
    inline QByteArray raw(QJsonDocument::JsonFormat format) const { return QJsonDocument(json()).toJson(format); }
};

#endif // JSONCONFIG_HPP

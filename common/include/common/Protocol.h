#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace hospital { namespace common {

struct Request
{
    QString module;
    QString action;
    QString token;
    QJsonObject headers;
    QJsonObject payload;
};

struct Response
{
    bool success = false;
    QString message;
    QJsonObject data;
};

class Protocol
{
public:
    static QByteArray encodeRequest(const Request& request);
    static QByteArray encodeResponse(const Response& response);

    static bool tryDecodeRequest(const QByteArray& bytes, Request* request, QString* error = nullptr);
    static bool tryDecodeResponse(const QByteArray& bytes, Response* response, QString* error = nullptr);

    static Request decodeRequest(const QByteArray& bytes);
    static Response decodeResponse(const QByteArray& bytes);
};

}} // namespace hospital::common

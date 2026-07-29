#include "common/Protocol.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace hospital::common {

namespace {

bool parseRootObject(const QByteArray& bytes, QJsonObject* root, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes.trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            *error = QStringLiteral("JSON解析失败：%1").arg(parseError.errorString());
        }
        return false;
    }

    if (!document.isObject()) {
        if (error) {
            *error = QStringLiteral("JSON根节点必须是对象。");
        }
        return false;
    }

    *root = document.object();
    return true;
}

} // namespace

QByteArray Protocol::encodeRequest(const Request& request)
{
    QJsonObject root;
    root["module"] = request.module;
    root["action"] = request.action;
    root["token"] = request.token;
    root["headers"] = request.headers;
    root["payload"] = request.payload;
    return QJsonDocument(root).toJson(QJsonDocument::Compact) + '\n';
}

QByteArray Protocol::encodeResponse(const Response& response)
{
    QJsonObject root;
    root["success"] = response.success;
    root["message"] = response.message;
    root["data"] = response.data;
    return QJsonDocument(root).toJson(QJsonDocument::Compact) + '\n';
}

Request Protocol::decodeRequest(const QByteArray& bytes)
{
    Request request;
    tryDecodeRequest(bytes, &request);
    return request;
}

Response Protocol::decodeResponse(const QByteArray& bytes)
{
    Response response;
    tryDecodeResponse(bytes, &response);
    return response;
}

bool Protocol::tryDecodeRequest(const QByteArray& bytes, Request* request, QString* error)
{
    if (!request) {
        if (error) {
            *error = QStringLiteral("请求输出参数不能为空。");
        }
        return false;
    }

    QJsonObject root;
    if (!parseRootObject(bytes, &root, error)) {
        return false;
    }

    Request decoded;
    decoded.module = root.value("module").toString().trimmed();
    decoded.action = root.value("action").toString().trimmed();
    decoded.token = root.value("token").toString();
    decoded.headers = root.value("headers").toObject();
    decoded.payload = root.value("payload").toObject();

    if (decoded.module.isEmpty() || decoded.action.isEmpty()) {
        if (error) {
            *error = QStringLiteral("请求缺少 module 或 action。");
        }
        return false;
    }

    *request = decoded;
    return true;
}

bool Protocol::tryDecodeResponse(const QByteArray& bytes, Response* response, QString* error)
{
    if (!response) {
        if (error) {
            *error = QStringLiteral("响应输出参数不能为空。");
        }
        return false;
    }

    QJsonObject root;
    if (!parseRootObject(bytes, &root, error)) {
        return false;
    }

    Response decoded;
    decoded.success = root.value("success").toBool();
    decoded.message = root.value("message").toString();
    decoded.data = root.value("data").toObject();

    *response = decoded;
    return true;
}

} // namespace hospital::common

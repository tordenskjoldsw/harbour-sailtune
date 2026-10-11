#include "jellyfin.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <memory>

namespace Jellyfin {

// A page of 500 items of a large library stays far below this.
const qint64 MaxAnswerBytes = 32 * 1024 * 1024;

namespace {

const char ClientName[] = "SailTune";
const char DeviceName[] = "Sailfish OS";

// Quotes in a header value would end it early; the values are ids, a
// token and fixed names, so they are only kept safe, not escaped.
QString headerSafe(QString value)
{
    value.remove(QLatin1Char('"'));
    value.remove(QLatin1Char('\r'));
    value.remove(QLatin1Char('\n'));
    return value;
}

} // namespace

QByteArray authorization(const Identity &identity)
{
    QString value = QStringLiteral("MediaBrowser Client=\"%1\", Device=\"%2\", DeviceId=\"%3\", Version=\"%4\"")
                        .arg(QLatin1String(ClientName), QLatin1String(DeviceName),
                             headerSafe(identity.deviceId), QStringLiteral(APP_VERSION));
    if (!identity.token.isEmpty())
        value += QStringLiteral(", Token=\"%1\"").arg(headerSafe(identity.token));
    return value.toUtf8();
}

QUrl url(const QString &address, const QString &path, const Query &query)
{
    QUrl result(address + path);
    if (!query.isEmpty()) {
        QUrlQuery urlQuery;
        urlQuery.setQueryItems(query);
        result.setQuery(urlQuery);
    }
    return result;
}

QString normalizedAddress(const QString &typed)
{
    QString address = typed.trimmed();
    if (!address.contains(QStringLiteral("://")))
        address.prepend(QStringLiteral("http://"));
    while (address.endsWith(QLatin1Char('/')))
        address.chop(1);
    const QUrl parsed(address, QUrl::StrictMode);
    if (!parsed.isValid() || parsed.host().isEmpty()
        || (parsed.scheme() != QLatin1String("http") && parsed.scheme() != QLatin1String("https")))
        return QString();
    return address;
}

void request(QNetworkAccessManager *network, const QString &method, const QUrl &url,
             const Identity &identity, const QByteArray &body, const JsonHandler &handler)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", authorization(identity));
    request.setRawHeader("Accept", "application/json");
    QNetworkReply *reply;
    if (method == QLatin1String("POST")) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = network->post(request, body);
    } else {
        reply = network->get(request);
    }
    // Stops reading an answer that grows beyond the limit.
    auto tooLarge = std::make_shared<bool>(false);
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply,
                     [reply, tooLarge](qint64 received, qint64 total) {
                         if (received > MaxAnswerBytes || total > MaxAnswerBytes) {
                             *tooLarge = true;
                             reply->abort();
                         }
                     });
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, tooLarge, handler] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (*tooLarge) {
            handler(Failure::BadAnswer, QJsonObject());
            return;
        }
        if (status == 401 || status == 403) {
            handler(Failure::Unauthorized, QJsonObject());
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            handler(Failure::Network, QJsonObject());
            return;
        }
        const QByteArray data = reply->readAll();
        // Some calls answer with no content on success.
        if (data.isEmpty()) {
            handler(Failure::None, QJsonObject());
            return;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            handler(Failure::BadAnswer, QJsonObject());
            return;
        }
        handler(Failure::None, document.object());
    });
}

} // namespace Jellyfin

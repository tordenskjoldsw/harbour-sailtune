#ifndef JELLYFIN_H
#define JELLYFIN_H

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QPair>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkReply;

// The few Jellyfin calls SailTune makes, with the header Jellyfin expects
// and a size limit on every answer, which is untrusted input.
namespace Jellyfin {

using Query = QList<QPair<QString, QString>>;

struct Identity {
    QString deviceId;
    QString token;
};

// The Authorization header: who the client is and, once logged in, its
// token.
QByteArray authorization(const Identity &identity);

QUrl url(const QString &address, const QString &path, const Query &query = Query());

// Normalizes what the user typed: a scheme is added when missing and a
// trailing slash removed. Empty when it is no http or https address.
QString normalizedAddress(const QString &typed);

enum class Failure {
    None,
    Network,
    // The server refused the token or the credentials.
    Unauthorized,
    // An answer that is no JSON object, too large, or misses a field, or
    // an error status other than a refused token.
    BadAnswer
};

using JsonHandler = std::function<void(Failure failure, const QJsonObject &answer)>;

// GET, POST or DELETE with a JSON answer of at most MaxAnswerBytes. The handler
// runs once, on the thread of the manager.
void request(QNetworkAccessManager *network, const QString &method, const QUrl &url,
             const Identity &identity, const QByteArray &body, const JsonHandler &handler);

extern const qint64 MaxAnswerBytes;

} // namespace Jellyfin

#endif // JELLYFIN_H

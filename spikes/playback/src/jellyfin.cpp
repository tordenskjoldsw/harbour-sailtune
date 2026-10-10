#include "jellyfin.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrlQuery>
#include <QUuid>

#include <memory>

namespace {

const int AlbumLimit = 300;
const qint64 TicksPerSecond = 10000000;

// A file extension from Jellyfin's container name, which lists aliases for
// MP4 audio ("mov,mp4,m4a,...").
QString extension(const QString &container)
{
    const QString first = container.section(QLatin1Char(','), 0, 0).toLower();
    if (container.contains(QLatin1String("m4a")) || first == QLatin1String("mov")
        || first == QLatin1String("mp4"))
        return QStringLiteral("m4a");
    QString clean;
    for (const QChar character : first) {
        if (character.isLetterOrNumber())
            clean.append(character);
    }
    return clean.isEmpty() ? QStringLiteral("bin") : clean;
}

QString httpStatus(QNetworkReply *reply)
{
    const QVariant code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    return code.isValid() ? QStringLiteral("HTTP %1").arg(code.toInt()) : reply->errorString();
}

} // namespace

Jellyfin::Jellyfin(QObject *parent)
    : QObject(parent)
{
    // Qt 5.6 has no QUuid::WithoutBraces.
    m_deviceId = QUuid::createUuid().toString().remove(QLatin1Char('{')).remove(QLatin1Char('}'));
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_logPath = dir + QStringLiteral("/log.txt");
}

bool Jellyfin::loggedIn() const
{
    return !m_token.isEmpty();
}

bool Jellyfin::busy() const
{
    return m_busy;
}

QString Jellyfin::error() const
{
    return m_error;
}

QByteArray Jellyfin::authorization() const
{
    QString value = QStringLiteral("MediaBrowser Client=\"Music spike\", Device=\"Jolla Phone\", "
                                   "DeviceId=\"%1\", Version=\"0.0.1\"")
                        .arg(m_deviceId);
    if (!m_token.isEmpty())
        value += QStringLiteral(", Token=\"%1\"").arg(m_token);
    return value.toUtf8();
}

void Jellyfin::login(const QString &server, const QString &user, const QString &password)
{
    m_server = server.trimmed();
    while (m_server.endsWith(QLatin1Char('/')))
        m_server.chop(1);
    m_token.clear();
    emit loggedInChanged();
    setError(QString());
    setBusy(true);

    QNetworkRequest request(QUrl(m_server + QStringLiteral("/Users/AuthenticateByName")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", authorization());
    QJsonObject body;
    body.insert(QStringLiteral("Username"), user);
    body.insert(QStringLiteral("Pw"), password);
    QNetworkReply *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            setError(tr("Login failed: %1").arg(httpStatus(reply)));
            log(QStringLiteral("login failed: %1").arg(httpStatus(reply)));
            return;
        }
        const QJsonObject result = QJsonDocument::fromJson(reply->readAll()).object();
        m_token = result.value(QStringLiteral("AccessToken")).toString();
        m_userId = result.value(QStringLiteral("User")).toObject().value(QStringLiteral("Id")).toString();
        if (m_token.isEmpty() || m_userId.isEmpty()) {
            m_token.clear();
            setError(tr("Login failed: unexpected answer"));
            log(QStringLiteral("login failed: unexpected answer"));
            return;
        }
        emit loggedInChanged();
        QNetworkReply *info = get(QStringLiteral("/System/Info/Public"), {});
        connect(info, &QNetworkReply::finished, this, [this, info] {
            info->deleteLater();
            const QJsonObject system = QJsonDocument::fromJson(info->readAll()).object();
            log(QStringLiteral("logged in; server version %1, scheme %2")
                    .arg(system.value(QStringLiteral("Version")).toString(), QUrl(m_server).scheme()));
        });
    });
}

QNetworkReply *Jellyfin::get(const QString &path, const QList<QPair<QString, QString>> &query)
{
    QUrl url(m_server + path);
    QUrlQuery urlQuery;
    urlQuery.setQueryItems(query);
    url.setQuery(urlQuery);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", authorization());
    return m_network.get(request);
}

void Jellyfin::loadAlbums(const QString &searchTerm)
{
    QList<QPair<QString, QString>> query = {
        {QStringLiteral("userId"), m_userId},
        {QStringLiteral("IncludeItemTypes"), QStringLiteral("MusicAlbum")},
        {QStringLiteral("Recursive"), QStringLiteral("true")},
        {QStringLiteral("SortBy"), QStringLiteral("AlbumArtist,SortName")},
        {QStringLiteral("Limit"), QString::number(AlbumLimit)},
    };
    if (!searchTerm.trimmed().isEmpty())
        query.append({QStringLiteral("SearchTerm"), searchTerm.trimmed()});
    setBusy(true);
    QElapsedTimer timer;
    timer.start();
    QNetworkReply *reply = get(QStringLiteral("/Items"), query);
    connect(reply, &QNetworkReply::finished, this, [this, reply, timer] {
        reply->deleteLater();
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            setError(tr("Albums failed: %1").arg(httpStatus(reply)));
            log(QStringLiteral("albums failed: %1").arg(httpStatus(reply)));
            return;
        }
        const QJsonObject result = QJsonDocument::fromJson(reply->readAll()).object();
        QVariantList albums;
        for (const QJsonValue &value : result.value(QStringLiteral("Items")).toArray()) {
            const QJsonObject item = value.toObject();
            QVariantMap album;
            album.insert(QStringLiteral("id"), item.value(QStringLiteral("Id")).toString());
            album.insert(QStringLiteral("name"), item.value(QStringLiteral("Name")).toString());
            album.insert(QStringLiteral("artist"), item.value(QStringLiteral("AlbumArtist")).toString());
            album.insert(QStringLiteral("year"), item.value(QStringLiteral("ProductionYear")).toInt());
            albums.append(album);
        }
        log(QStringLiteral("albums: %1 of %2 in %3 ms")
                .arg(albums.size())
                .arg(result.value(QStringLiteral("TotalRecordCount")).toInt())
                .arg(timer.elapsed()));
        emit albumsLoaded(albums);
    });
}

void Jellyfin::loadTracks(const QString &albumId)
{
    setBusy(true);
    QNetworkReply *reply = get(QStringLiteral("/Items"), {
        {QStringLiteral("userId"), m_userId},
        {QStringLiteral("ParentId"), albumId},
        {QStringLiteral("IncludeItemTypes"), QStringLiteral("Audio")},
        {QStringLiteral("Recursive"), QStringLiteral("true")},
        {QStringLiteral("SortBy"), QStringLiteral("ParentIndexNumber,IndexNumber,SortName")},
        {QStringLiteral("Fields"), QStringLiteral("MediaSources")},
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            setError(tr("Tracks failed: %1").arg(httpStatus(reply)));
            log(QStringLiteral("tracks failed: %1").arg(httpStatus(reply)));
            return;
        }
        const QJsonObject result = QJsonDocument::fromJson(reply->readAll()).object();
        QVariantList tracks;
        for (const QJsonValue &value : result.value(QStringLiteral("Items")).toArray()) {
            const QJsonObject item = value.toObject();
            const QJsonObject source = item.value(QStringLiteral("MediaSources")).toArray().first().toObject();
            QString codec;
            int sampleRate = 0;
            int bitDepth = 0;
            for (const QJsonValue &stream : source.value(QStringLiteral("MediaStreams")).toArray()) {
                const QJsonObject media = stream.toObject();
                if (media.value(QStringLiteral("Type")).toString() == QLatin1String("Audio")) {
                    codec = media.value(QStringLiteral("Codec")).toString();
                    sampleRate = media.value(QStringLiteral("SampleRate")).toInt();
                    bitDepth = media.value(QStringLiteral("BitDepth")).toInt();
                    break;
                }
            }
            const QJsonArray artists = item.value(QStringLiteral("Artists")).toArray();
            QVariantMap track;
            track.insert(QStringLiteral("id"), item.value(QStringLiteral("Id")).toString());
            track.insert(QStringLiteral("name"), item.value(QStringLiteral("Name")).toString());
            track.insert(QStringLiteral("number"), item.value(QStringLiteral("IndexNumber")).toInt());
            track.insert(QStringLiteral("artist"), artists.isEmpty()
                         ? item.value(QStringLiteral("AlbumArtist")).toString()
                         : artists.first().toString());
            track.insert(QStringLiteral("album"), item.value(QStringLiteral("Album")).toString());
            track.insert(QStringLiteral("seconds"),
                         int(qint64(item.value(QStringLiteral("RunTimeTicks")).toDouble()) / TicksPerSecond));
            track.insert(QStringLiteral("container"), extension(source.value(QStringLiteral("Container")).toString()));
            track.insert(QStringLiteral("codec"), codec);
            track.insert(QStringLiteral("kbps"), source.value(QStringLiteral("Bitrate")).toInt() / 1000);
            track.insert(QStringLiteral("sampleRate"), sampleRate);
            track.insert(QStringLiteral("bitDepth"), bitDepth);
            track.insert(QStringLiteral("bytes"), source.value(QStringLiteral("Size")).toDouble());
            tracks.append(track);
        }
        emit tracksLoaded(tracks);
    });
}

QUrl Jellyfin::streamUrl(const QString &trackId) const
{
    QUrl url(m_server + QStringLiteral("/Audio/%1/stream").arg(trackId));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("static"), QStringLiteral("true"));
    // QtMultimedia sends no custom headers, so the token goes in the URL.
    query.addQueryItem(QStringLiteral("api_key"), m_token);
    url.setQuery(query);
    return url;
}

QString Jellyfin::musicDirectory() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/music");
    QDir().mkpath(dir);
    return dir;
}

QUrl Jellyfin::localFile(const QString &trackId, const QString &container) const
{
    const QString path = musicDirectory() + QLatin1Char('/') + trackId + QLatin1Char('.') + container;
    return QFile::exists(path) ? QUrl::fromLocalFile(path) : QUrl();
}

void Jellyfin::download(const QVariantMap &track)
{
    const QString trackId = track.value(QStringLiteral("id")).toString();
    const QString container = track.value(QStringLiteral("container")).toString();
    // Only what the offline list shows; no token, no address.
    QJsonObject details;
    for (const char *key : {"id", "name", "artist", "album", "number", "seconds", "codec", "container"})
        details.insert(QLatin1String(key), QJsonValue::fromVariant(track.value(QLatin1String(key))));
    const QByteArray detailsJson = QJsonDocument(details).toJson(QJsonDocument::Compact);
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    QNetworkReply *reply = get(QStringLiteral("/Audio/%1/stream").arg(trackId),
                               {{QStringLiteral("static"), QStringLiteral("true")}});
    connect(reply, &QNetworkReply::finished, this, [this, reply, trackId, container, timer, detailsJson] {
        reply->deleteLater();
        const int milliseconds = int(timer->elapsed());
        if (reply->error() != QNetworkReply::NoError) {
            log(QStringLiteral("download %1 failed: %2").arg(trackId.left(8), httpStatus(reply)));
            emit downloadFinished(trackId, false, 0, milliseconds);
            return;
        }
        const QByteArray data = reply->readAll();
        const QString path = musicDirectory() + QLatin1Char('/') + trackId + QLatin1Char('.') + container;
        QFile file(path + QStringLiteral(".part"));
        const bool written = file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        file.close();
        bool ok = written && (QFile::remove(path) || !QFile::exists(path))
            && QFile::rename(path + QStringLiteral(".part"), path);
        if (ok) {
            QFile detailsFile(musicDirectory() + QLatin1Char('/') + trackId + QStringLiteral(".json"));
            ok = detailsFile.open(QIODevice::WriteOnly) && detailsFile.write(detailsJson) == detailsJson.size();
        }
        log(QStringLiteral("download %1 %2: %3 MB in %4 s, %5")
                .arg(trackId.left(8), container)
                .arg(data.size() / 1048576.0, 0, 'f', 1)
                .arg(milliseconds / 1000.0, 0, 'f', 1)
                .arg(ok ? QStringLiteral("saved") : QStringLiteral("not saved")));
        emit downloadFinished(trackId, ok, data.size(), milliseconds);
    });
}

QVariantList Jellyfin::downloads() const
{
    QVariantList tracks;
    const QDir dir(musicDirectory());
    for (const QFileInfo &file : dir.entryInfoList(QDir::Files, QDir::Name)) {
        const QString suffix = file.suffix();
        if (suffix == QLatin1String("json") || suffix == QLatin1String("part"))
            continue;
        QVariantMap track;
        QFile detailsFile(dir.filePath(file.completeBaseName() + QStringLiteral(".json")));
        if (detailsFile.open(QIODevice::ReadOnly))
            track = QJsonDocument::fromJson(detailsFile.readAll()).object().toVariantMap();
        if (track.isEmpty()) {
            track.insert(QStringLiteral("id"), file.completeBaseName());
            track.insert(QStringLiteral("name"), QStringLiteral("Track %1").arg(file.completeBaseName().left(8)));
            track.insert(QStringLiteral("album"), QStringLiteral("Without details"));
            track.insert(QStringLiteral("artist"), QString());
            track.insert(QStringLiteral("number"), 0);
            track.insert(QStringLiteral("seconds"), 0);
            track.insert(QStringLiteral("codec"), suffix);
        }
        track.insert(QStringLiteral("container"), suffix);
        tracks.append(track);
    }
    return tracks;
}

void Jellyfin::log(const QString &line)
{
    QFile file(m_logPath);
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(Qt::ISODate) << "  " << line << "\n";
}

QString Jellyfin::logText() const
{
    QFile file(m_logPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

void Jellyfin::clearLog()
{
    QFile::remove(m_logPath);
}

void Jellyfin::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Jellyfin::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}

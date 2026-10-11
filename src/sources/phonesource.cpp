#include "phonesource.h"

#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtConcurrent>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

const char PhoneSource::Id[] = "phone";

namespace {

const qint64 MaxIndexBytes = 64 * 1024 * 1024;
const int IndexTimeoutMs = 30000;
const int JsonLdFormat = 2;
const QString UnknownAlbumId = QStringLiteral("unknown-album");

// Every music file the index knows, with the album, its artist and the
// disc, as linked resources; JSON-LD keeps the links as node ids.
const char IndexQuery[] =
    "CONSTRUCT {"
    "  ?song nie:title ?title ; nfo:duration ?duration ; nmm:trackNumber ?track ;"
    "        nie:isStoredAs ?file ; nmm:musicAlbum ?album ; nmm:artist ?artist ;"
    "        nmm:musicAlbumDisc ?disc ."
    "  ?file nie:url ?url ."
    "  ?album nie:title ?albumTitle ; nmm:albumArtist ?albumArtist ."
    "  ?albumArtist nmm:artistName ?albumArtistName ."
    "  ?artist nmm:artistName ?artistName ."
    "  ?disc nmm:setNumber ?discNumber ."
    "} WHERE {"
    "  ?song a nmm:MusicPiece ; nie:isStoredAs ?file ."
    "  ?file nie:url ?url ."
    "  OPTIONAL { ?song nie:title ?title }"
    "  OPTIONAL { ?song nfo:duration ?duration }"
    "  OPTIONAL { ?song nmm:trackNumber ?track }"
    "  OPTIONAL { ?song nmm:artist ?artist . ?artist nmm:artistName ?artistName }"
    "  OPTIONAL { ?song nmm:musicAlbumDisc ?disc . ?disc nmm:setNumber ?discNumber }"
    "  OPTIONAL { ?song nmm:musicAlbum ?album . ?album nie:title ?albumTitle"
    "    OPTIONAL { ?album nmm:albumArtist ?albumArtist ."
    "               ?albumArtist nmm:artistName ?albumArtistName } }"
    "}";

// Reads the pipe until the index closes it, bounded in size and time.
QByteArray readPipe(int fd, QString *error)
{
    QByteArray data;
    char buffer[65536];
    for (;;) {
        pollfd waiting{fd, POLLIN, 0};
        const int ready = ::poll(&waiting, 1, IndexTimeoutMs);
        if (ready <= 0) {
            *error = QStringLiteral("timeout");
            break;
        }
        const ssize_t got = ::read(fd, buffer, sizeof(buffer));
        if (got <= 0)
            break;
        if (data.size() + got > MaxIndexBytes) {
            *error = QStringLiteral("too large");
            break;
        }
        data.append(buffer, int(got));
    }
    ::close(fd);
    return data;
}

// A property of a JSON-LD node, whatever prefix form the serializer chose.
QJsonValue value(const QJsonObject &node, const QString &localName)
{
    for (auto it = node.constBegin(); it != node.constEnd(); ++it) {
        const QString &key = it.key();
        if (key == localName || key.endsWith(QLatin1Char(':') + localName)
            || key.endsWith(QLatin1Char('#') + localName)) {
            QJsonValue result = it.value();
            if (result.isArray())
                result = result.toArray().first();
            if (result.isObject()) {
                const QJsonObject object = result.toObject();
                if (object.contains(QStringLiteral("@value")))
                    return object.value(QStringLiteral("@value"));
                return object.value(QStringLiteral("@id"));
            }
            return result;
        }
    }
    return QJsonValue();
}

QString string(const QJsonObject &node, const QString &localName)
{
    const QJsonValue found = value(node, localName);
    return found.isDouble() ? QString::number(found.toDouble()) : found.toString();
}

int number(const QJsonObject &node, const QString &localName)
{
    const QJsonValue found = value(node, localName);
    return found.isDouble() ? found.toInt() : found.toString().toInt();
}

// The name the media art storage gives a text: lower case without
// bracketed parts, special characters and repeated spaces; checked on the
// device against the art the system wrote.
QString mediaArtHash(const QString &text)
{
    QString normalized = text.toLower();
    normalized.remove(QRegularExpression(QStringLiteral("\\([^)]*\\)|\\[[^\\]]*\\]|\\{[^}]*\\}|<[^>]*>")));
    normalized.remove(QRegularExpression(QStringLiteral("[_!@#$^&*+=|\\\\/\"'?~]")));
    normalized = normalized.simplified();
    if (normalized.isEmpty())
        normalized = QStringLiteral(" ");
    return QString::fromLatin1(
        QCryptographicHash::hash(normalized.toUtf8(), QCryptographicHash::Md5).toHex());
}

QString albumArt(const QString &artist, const QString &album)
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/media-art/");
    const QString albumHash = mediaArtHash(album);
    for (const QString &name : {QStringLiteral("album-%1-%2.jpeg").arg(mediaArtHash(artist), albumHash),
                                QStringLiteral("album-%1-%2.jpeg").arg(albumHash, mediaArtHash(QString()))}) {
        if (QFile::exists(directory + name))
            return directory + name;
    }
    return QString();
}

LibrarySnapshot snapshotFromJsonLd(const QByteArray &data)
{
    LibrarySnapshot snapshot;
    const QJsonDocument document = QJsonDocument::fromJson(data);
    QJsonArray nodes;
    if (document.isArray())
        nodes = document.array();
    else if (document.object().contains(QStringLiteral("@graph")))
        nodes = document.object().value(QStringLiteral("@graph")).toArray();
    else if (!document.object().isEmpty())
        nodes.append(document.object());

    QHash<QString, QJsonObject> byId;
    for (const QJsonValue &node : nodes)
        byId.insert(node.toObject().value(QStringLiteral("@id")).toString(), node.toObject());

    QHash<QString, LibraryArtist> artists;
    QHash<QString, LibraryAlbum> albums;
    for (auto it = byId.constBegin(); it != byId.constEnd(); ++it) {
        const QJsonObject &song = it.value();
        const QString fileId = string(song, QStringLiteral("isStoredAs"));
        if (fileId.isEmpty())
            continue;
        LibraryTrack track;
        track.id = it.key();
        track.location = string(byId.value(fileId), QStringLiteral("url"));
        track.title = string(song, QStringLiteral("title"));
        if (track.title.isEmpty())
            track.title = QUrl(track.location).fileName();
        track.number = number(song, QStringLiteral("trackNumber"));
        track.seconds = number(song, QStringLiteral("duration"));
        const QString artistId = string(song, QStringLiteral("artist"));
        track.artist = string(byId.value(artistId), QStringLiteral("artistName"));
        track.disc = number(byId.value(string(song, QStringLiteral("musicAlbumDisc"))),
                            QStringLiteral("setNumber"));

        const QString albumId = string(song, QStringLiteral("musicAlbum"));
        track.albumId = albumId.isEmpty() ? UnknownAlbumId : albumId;
        const QJsonObject albumNode = byId.value(albumId);
        track.album = string(albumNode, QStringLiteral("title"));

        if (!albums.contains(track.albumId)) {
            LibraryAlbum album;
            album.id = track.albumId;
            album.title = track.album;
            const QString albumArtistId = string(albumNode, QStringLiteral("albumArtist"));
            album.artistId = albumArtistId.isEmpty() ? artistId : albumArtistId;
            album.artist = string(byId.value(album.artistId), QStringLiteral("artistName"));
            album.image = albumArt(album.artist, album.title);
            albums.insert(album.id, album);
        }
        const LibraryAlbum &album = albums.value(track.albumId);
        if (!album.artistId.isEmpty() && !artists.contains(album.artistId)) {
            LibraryArtist artist;
            artist.id = album.artistId;
            artist.name = album.artist;
            artist.image = album.image;
            artists.insert(artist.id, artist);
        }
        snapshot.tracks.append(track);
    }
    for (const LibraryAlbum &album : albums)
        snapshot.albums.append(album);
    for (const LibraryArtist &artist : artists)
        snapshot.artists.append(artist);
    return snapshot;
}

} // namespace

PhoneSource::PhoneSource(const QString &directory, QObject *parent)
    : Source(QString::fromLatin1(Id), directory, parent)
    , m_receiver(std::make_shared<Receiver>())
{
    m_receiver->source = this;
}

PhoneSource::~PhoneSource()
{
    // Waits for a handover in progress; a call it already queued is
    // dropped with this object.
    QMutexLocker lock(&m_receiver->mutex);
    m_receiver->source = nullptr;
}

QString PhoneSource::kind() const
{
    return QStringLiteral("phone");
}

QString PhoneSource::name() const
{
    return tr("This phone");
}

void PhoneSource::refresh()
{
    if (refreshing())
        return;
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        setProblem(tr("The phone's music index could not be read"));
        return;
    }
    setRefreshing(true);
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Tracker3.Miner.Files"),
        QStringLiteral("/org/freedesktop/Tracker3/Endpoint"),
        QStringLiteral("org.freedesktop.Tracker3.Endpoint"), QStringLiteral("Serialize"));
    {
        // The message holds its own copy of the write end; once it is sent
        // only the index keeps one, and closing it ends the read.
        QDBusUnixFileDescriptor writeEnd(fds[1]);
        ::close(fds[1]);
        call << QString::fromLatin1(IndexQuery) << QVariant::fromValue(writeEnd) << 0
             << JsonLdFormat << QVariantMap();
    }
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, watcher, &QObject::deleteLater);
    const int readFd = fds[0];
    const std::shared_ptr<Receiver> receiver = m_receiver;
    QtConcurrent::run([receiver, readFd] {
        QString error;
        const QByteArray data = readPipe(readFd, &error);
        QMutexLocker lock(&receiver->mutex);
        if (receiver->source) {
            QMetaObject::invokeMethod(receiver->source, "onIndexRead", Qt::QueuedConnection,
                                      Q_ARG(QByteArray, data), Q_ARG(QString, error));
        }
    });
}

void PhoneSource::onIndexRead(const QByteArray &data, const QString &error)
{
    setRefreshing(false);
    if (!error.isEmpty() || data.isEmpty()) {
        setProblem(tr("The phone's music index could not be read"));
        return;
    }
    setProblem(QString());
    store(snapshotFromJsonLd(data));
}

QUrl PhoneSource::trackUrl(const QVariantMap &track) const
{
    return QUrl(track.value(QStringLiteral("location")).toString());
}

QUrl PhoneSource::imageUrl(const QString &image, int size) const
{
    Q_UNUSED(size)
    return image.isEmpty() ? QUrl() : QUrl::fromLocalFile(image);
}

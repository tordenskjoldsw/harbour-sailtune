#include "probe.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace {

const qint64 MaxResultBytes = 32 * 1024 * 1024;
const int ReadTimeoutMs = 20000;
const quint16 JellyfinDiscoveryPort = 7359;

const char IndexQuery[] =
    "CONSTRUCT {"
    "  ?song nie:title ?title ; nfo:duration ?duration ; nmm:trackNumber ?track ;"
    "        nie:isStoredAs ?file ; nmm:musicAlbum ?album ; nmm:artist ?artist ."
    "  ?file nie:url ?url ."
    "  ?album nie:title ?albumTitle ."
    "  ?artist nmm:artistName ?artistName ."
    "} WHERE {"
    "  ?song a nmm:MusicPiece ; nie:isStoredAs ?file ."
    "  ?file nie:url ?url ."
    "  OPTIONAL { ?song nie:title ?title }"
    "  OPTIONAL { ?song nfo:duration ?duration }"
    "  OPTIONAL { ?song nmm:trackNumber ?track }"
    "  OPTIONAL { ?song nmm:musicAlbum ?album . ?album nie:title ?albumTitle }"
    "  OPTIONAL { ?song nmm:artist ?artist . ?artist nmm:artistName ?artistName }"
    "}";

// Reads the pipe until the writer closes it, with a size cap and a timeout
// for a writer that never starts.
QByteArray readPipe(int fd, QString *error)
{
    QByteArray data;
    char buffer[65536];
    for (;;) {
        pollfd waiting{fd, POLLIN, 0};
        const int ready = ::poll(&waiting, 1, ReadTimeoutMs);
        if (ready == 0) {
            *error = QStringLiteral("timeout after %1 s").arg(ReadTimeoutMs / 1000);
            break;
        }
        if (ready < 0) {
            *error = QStringLiteral("poll failed");
            break;
        }
        const ssize_t got = ::read(fd, buffer, sizeof(buffer));
        if (got <= 0)
            break;
        if (data.size() + got > MaxResultBytes) {
            *error = QStringLiteral("result above %1 MB").arg(MaxResultBytes / 1048576);
            break;
        }
        data.append(buffer, int(got));
    }
    ::close(fd);
    return data;
}

// A property of a JSON-LD node, whatever prefix or IRI form the
// serializer chose for it.
QJsonValue jsonProperty(const QJsonObject &node, const QString &localName)
{
    for (auto it = node.constBegin(); it != node.constEnd(); ++it) {
        const QString &key = it.key();
        if (key == localName || key.endsWith(QLatin1Char(':') + localName)
            || key.endsWith(QLatin1Char('#') + localName)) {
            QJsonValue value = it.value();
            if (value.isArray())
                value = value.toArray().first();
            if (value.isObject()) {
                const QJsonObject object = value.toObject();
                if (object.contains(QStringLiteral("@value")))
                    return object.value(QStringLiteral("@value"));
                if (object.contains(QStringLiteral("@id")))
                    return object.value(QStringLiteral("@id"));
            }
            return value;
        }
    }
    return QJsonValue();
}

QString text(const QJsonValue &value)
{
    return value.isDouble() ? QString::number(value.toDouble()) : value.toString();
}

} // namespace

Probe::Probe(QObject *parent)
    : QObject(parent)
{
    connect(&m_udp, &QUdpSocket::readyRead, this, [this] {
        while (m_udp.hasPendingDatagrams()) {
            QByteArray datagram(int(m_udp.pendingDatagramSize()), Qt::Uninitialized);
            QHostAddress sender;
            m_udp.readDatagram(datagram.data(), datagram.size(), &sender);
            const QJsonObject server = QJsonDocument::fromJson(datagram).object();
            add(QStringLiteral("server answered from %1: name \"%2\", address %3")
                    .arg(sender.toString(), server.value(QStringLiteral("Name")).toString(),
                         server.value(QStringLiteral("Address")).toString()));
        }
    });
}

QString Probe::report() const
{
    return m_report;
}

QVariantList Probe::songs() const
{
    return m_songs;
}

void Probe::add(const QString &line)
{
    m_report = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")) + QStringLiteral("  ")
        + line + QLatin1Char('\n') + m_report;
    emit reportChanged();
}

void Probe::clear()
{
    m_report.clear();
    emit reportChanged();
}

void Probe::queryIndex(int format)
{
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        add(QStringLiteral("index: pipe failed"));
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Tracker3.Miner.Files"),
        QStringLiteral("/org/freedesktop/Tracker3/Endpoint"),
        QStringLiteral("org.freedesktop.Tracker3.Endpoint"), QStringLiteral("Serialize"));
    {
        // The descriptor object duplicates the write end; only the server
        // keeps it once the call is sent, so its close ends our read.
        QDBusUnixFileDescriptor writeEnd(fds[1]);
        ::close(fds[1]);
        call << QString::fromLatin1(IndexQuery) << QVariant::fromValue(writeEnd) << 0 << format
             << QVariantMap();
    }
    add(QStringLiteral("index: Serialize sent, format %1").arg(format));
    QElapsedTimer timer;
    timer.start();
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    call = QDBusMessage();
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, timer] {
        watcher->deleteLater();
        const QDBusPendingReply<> reply = *watcher;
        if (reply.isError())
            add(QStringLiteral("index: D-Bus error %1: %2")
                    .arg(reply.error().name(), reply.error().message()));
        else
            add(QStringLiteral("index: D-Bus reply after %1 ms").arg(timer.elapsed()));
    });
    const int readFd = fds[0];
    QtConcurrent::run([this, readFd, timer] {
        QString error;
        const QByteArray data = readPipe(readFd, &error);
        QMetaObject::invokeMethod(this, "onSerialized", Qt::QueuedConnection,
                                  Q_ARG(QByteArray, data), Q_ARG(int, int(timer.elapsed())),
                                  Q_ARG(QString, error));
    });
}

void Probe::onSerialized(const QByteArray &data, int milliseconds, const QString &readError)
{
    add(QStringLiteral("index: %1 bytes in %2 ms%3")
            .arg(data.size())
            .arg(milliseconds)
            .arg(readError.isEmpty() ? QString() : QStringLiteral(", ") + readError));
    if (data.isEmpty())
        return;
    // The start of the raw answer, to see the serializer's shape.
    add(QStringLiteral("raw start: ") + QString::fromUtf8(data.left(600)));
    parseJsonLd(data);
}

void Probe::parseJsonLd(const QByteArray &data)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        add(QStringLiteral("JSON-LD: not JSON (%1)").arg(parseError.errorString()));
        return;
    }
    QJsonArray nodes;
    if (document.isArray())
        nodes = document.array();
    else if (document.object().contains(QStringLiteral("@graph")))
        nodes = document.object().value(QStringLiteral("@graph")).toArray();
    else
        nodes.append(document.object());

    QHash<QString, QJsonObject> byId;
    for (const QJsonValue &value : nodes) {
        const QJsonObject node = value.toObject();
        byId.insert(node.value(QStringLiteral("@id")).toString(), node);
    }
    QVariantList songs;
    for (const QJsonObject &node : byId) {
        const QString file = text(jsonProperty(node, QStringLiteral("isStoredAs")));
        if (file.isEmpty())
            continue;
        QVariantMap song;
        song.insert(QStringLiteral("title"), text(jsonProperty(node, QStringLiteral("title"))));
        song.insert(QStringLiteral("seconds"), jsonProperty(node, QStringLiteral("duration")).toVariant());
        song.insert(QStringLiteral("track"), jsonProperty(node, QStringLiteral("trackNumber")).toVariant());
        song.insert(QStringLiteral("url"), text(jsonProperty(byId.value(file), QStringLiteral("url"))));
        const QString album = text(jsonProperty(node, QStringLiteral("musicAlbum")));
        song.insert(QStringLiteral("album"), text(jsonProperty(byId.value(album), QStringLiteral("title"))));
        const QString artist = text(jsonProperty(node, QStringLiteral("artist")));
        song.insert(QStringLiteral("artist"), text(jsonProperty(byId.value(artist), QStringLiteral("artistName"))));
        songs.append(song);
    }
    m_songs = songs;
    emit songsChanged();
    add(QStringLiteral("JSON-LD: %1 nodes, %2 songs").arg(byId.size()).arg(songs.size()));
}

void Probe::countMediaArt()
{
    const QDir dir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                   + QStringLiteral("/media-art"));
    const QStringList files = dir.entryList(QDir::Files);
    add(QStringLiteral("media art: %1 files in %2%3")
            .arg(files.size())
            .arg(dir.path())
            .arg(files.isEmpty() ? QString() : QStringLiteral(", e.g. ") + files.first()));
}

void Probe::discoverServers()
{
    if (m_udp.state() != QAbstractSocket::BoundState
        && !m_udp.bind(QHostAddress(QHostAddress::AnyIPv4), 0)) {
        add(QStringLiteral("discovery: bind failed: %1").arg(m_udp.errorString()));
        return;
    }
    const qint64 sent = m_udp.writeDatagram(QByteArrayLiteral("Who is JellyfinServer?"),
                                            QHostAddress::Broadcast, JellyfinDiscoveryPort);
    add(sent < 0 ? QStringLiteral("discovery: send failed: %1").arg(m_udp.errorString())
                 : QStringLiteral("discovery: asked the network, waiting 3 s"));
    QTimer::singleShot(3000, this, [this] { add(QStringLiteral("discovery: done")); });
}

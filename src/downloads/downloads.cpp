#include "downloads.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>

#include "../library/librarydatabase.h"
#include "../sources/source.h"
#include "../sources/sources.h"

namespace {

// Downloads pause before the phone's free space falls below this
// (PLAN.md section 14).
const qint64 ReserveBytes = qint64(2) * 1024 * 1024 * 1024;
// Far above a high-resolution FLAC track; a larger answer is not music.
const qint64 MaxFileBytes = qint64(2) * 1024 * 1024 * 1024;
// While paused: a fallback for a connection that stayed up while the
// server was missing, as WLAN away from home before the VPN is on.
const int RetryMs = 30000;
// After the phone reports a new connection, routes and the VPN settle.
const int OnlineDelayMs = 2000;
// A restored queue starts after the app has settled.
const int StartDelayMs = 3000;
const int MaxQueue = 20000;
const QString PartSuffix = QStringLiteral(".part");

QString id(const QVariantMap &track)
{
    return track.value(QStringLiteral("id")).toString();
}

// File names come from ids, never from titles; an id that is not plain is
// not used for a file at all.
bool safeId(const QString &trackId)
{
    static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
    return plain.match(trackId).hasMatch();
}

QString extensionFor(const QString &contentType)
{
    const QString type = contentType.section(QLatin1Char(';'), 0, 0).trimmed().toLower();
    if (type == QLatin1String("audio/flac") || type == QLatin1String("audio/x-flac"))
        return QStringLiteral("flac");
    if (type == QLatin1String("audio/mpeg"))
        return QStringLiteral("mp3");
    if (type == QLatin1String("audio/mp4") || type == QLatin1String("audio/x-m4a")
        || type == QLatin1String("audio/aac"))
        return QStringLiteral("m4a");
    if (type == QLatin1String("audio/ogg") || type == QLatin1String("audio/vorbis"))
        return QStringLiteral("ogg");
    if (type == QLatin1String("audio/opus"))
        return QStringLiteral("opus");
    if (type == QLatin1String("audio/wav") || type == QLatin1String("audio/x-wav"))
        return QStringLiteral("wav");
    return QStringLiteral("audio");
}

bool savedFileExists(Source *source, const QString &trackId)
{
    const QString file = source->library()->downloadedFile(trackId);
    return !file.isEmpty() && QFileInfo(file).isFile();
}

bool enoughSpace(const QString &directory, qint64 needed)
{
    const QStorageInfo storage(directory);
    return storage.isValid() && storage.bytesAvailable() - qMax<qint64>(0, needed) >= ReserveBytes;
}

// Errors of the connection, after which the same request may work later;
// everything else is the server's answer about this one file.
bool connectionLost(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::ServiceUnavailableError:
        return true;
    default:
        return false;
    }
}

} // namespace

Downloads::Downloads(Sources *sources, QObject *parent)
    : QObject(parent)
    , m_sources(sources)
{
    m_retry.setSingleShot(true);
    m_retry.setInterval(RetryMs);
    connect(&m_retry, &QTimer::timeout, this, &Downloads::resume);
    // A paused queue continues as soon as the phone reports a connection
    // instead of waiting for the next retry (seen on the device: up to a
    // minute with the retry alone).
    auto retrySoon = [this] {
        if (m_paused && !m_queue.isEmpty())
            m_retry.start(OnlineDelayMs);
    };
    connect(&m_networkState, &QNetworkConfigurationManager::onlineStateChanged, this,
            [retrySoon](bool online) {
                if (online)
                    retrySoon();
            });
    connect(&m_networkState, &QNetworkConfigurationManager::configurationChanged, this,
            [retrySoon](const QNetworkConfiguration &configuration) {
                if (configuration.state().testFlag(QNetworkConfiguration::Active))
                    retrySoon();
            });
    connect(&m_location, &DownloadLocation::changed, this, &Downloads::onLocationChanged);
    connect(m_sources, &Sources::allChanged, this, &Downloads::applyPresentRoots);
    // A forgotten server's downloads on the card go with it.
    connect(m_sources, &Sources::removed, this, [this](const QString &sourceId) {
        const QString directory = m_location.cardDirectoryFor(sourceId);
        if (!directory.isEmpty())
            QDir(directory).removeRecursively();
    });
    applyPresentRoots();
    loadQueue();
}

Downloads::~Downloads()
{
    // What is left stays queued, and the part file stays for the next
    // start.
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_file.close();
        m_queue.prepend(m_track);
    }
    saveQueue();
}

bool Downloads::running() const
{
    return !m_reply.isNull();
}

bool Downloads::paused() const
{
    return m_paused && !m_queue.isEmpty();
}

int Downloads::waiting() const
{
    return m_queue.size();
}

QString Downloads::current() const
{
    return m_track.value(QStringLiteral("title")).toString();
}

int Downloads::percent() const
{
    return m_expected > 0 ? int((m_offset + m_received) * 100 / m_expected) : 0;
}

QString Downloads::problem() const
{
    return m_problem;
}

int Downloads::revision() const
{
    return m_revision;
}

bool Downloads::onCard() const
{
    return m_location.onCard();
}

void Downloads::setOnCard(bool onCard)
{
    if (m_location.onCard() == onCard)
        return;
    // A part file stays on the side it was started on, where it is of no
    // use any more.
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply = nullptr;
        m_file.close();
        m_queue.prepend(m_track);
        m_track.clear();
    }
    if (m_source && !m_queue.isEmpty())
        removeParts(m_source, id(m_queue.first()));
    m_location.setOnCard(onCard);
}

bool Downloads::cardPresent() const
{
    return !m_location.cardRoot().isEmpty();
}

void Downloads::onLocationChanged()
{
    applyPresentRoots();
    bumpRevision();
    emit locationChanged();
    if (!running() && !m_queue.isEmpty())
        resume();
}

void Downloads::applyPresentRoots()
{
    const QStringList roots = m_location.presentRoots();
    for (Source *source : m_sources->list()) {
        if (source->canDownload())
            source->library()->setPresentRoots(roots);
    }
}

QString Downloads::spaceProblem() const
{
    return m_location.onCard() ? tr("Downloads paused: less than 2 GB free on the memory card")
                               : tr("Downloads paused: less than 2 GB free on the phone");
}

void Downloads::setProblem(const QString &problem)
{
    if (m_problem == problem)
        return;
    m_problem = problem;
    emit problemChanged();
}

void Downloads::bumpRevision()
{
    ++m_revision;
    emit revisionChanged();
}

QString Downloads::queuePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/download-queue.json");
}

QString Downloads::partPath(const QVariantMap &track) const
{
    return m_directory + QLatin1Char('/') + id(track) + PartSuffix;
}

void Downloads::removeParts(Source *source, const QString &trackId) const
{
    if (!safeId(trackId))
        return;
    QFile::remove(source->downloadDirectory() + QLatin1Char('/') + trackId + PartSuffix);
    const QString card = m_location.cardDirectoryFor(source->id());
    if (!card.isEmpty())
        QFile::remove(card + QLatin1Char('/') + trackId + PartSuffix);
}

void Downloads::saveQueue() const
{
    QList<QVariantMap> all = m_queue;
    if (!m_track.isEmpty())
        all.prepend(m_track);
    if (all.isEmpty() || !m_source) {
        QFile::remove(queuePath());
        return;
    }
    QJsonArray tracks;
    for (const QVariantMap &track : all) {
        QJsonObject entry;
        entry.insert(QStringLiteral("id"), id(track));
        entry.insert(QStringLiteral("title"), track.value(QStringLiteral("title")).toString());
        tracks.append(entry);
    }
    QJsonObject queue;
    queue.insert(QStringLiteral("source"), m_source->id());
    queue.insert(QStringLiteral("tracks"), tracks);
    QSaveFile file(queuePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(queue).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

void Downloads::loadQueue()
{
    QFile file(queuePath());
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024)
        return;
    const QJsonObject queue = QJsonDocument::fromJson(file.readAll()).object();
    Source *source = m_sources->source(queue.value(QStringLiteral("source")).toString());
    if (!source || !source->canDownload())
        return;
    m_source = source;
    const QJsonArray tracks = queue.value(QStringLiteral("tracks")).toArray();
    for (int i = 0; i < tracks.size() && i < MaxQueue; ++i) {
        const QVariantMap track = tracks.at(i).toObject().toVariantMap();
        if (safeId(id(track)))
            m_queue.append(track);
    }
    if (m_queue.isEmpty())
        return;
    connect(source, &Source::reachableChanged, this, [this, source] {
        if (source->reachable() && m_paused)
            resume();
    });
    m_paused = true;
    m_retry.start(StartDelayMs);
}

void Downloads::download(const QVariantList &tracks)
{
    Source *source = m_sources->active();
    if (!source || !source->canDownload())
        return;
    if (m_source && m_source != source && (running() || !m_queue.isEmpty())) {
        setProblem(tr("Wait for the downloads of the other source to finish"));
        return;
    }
    if (m_source != source) {
        if (m_source)
            disconnect(m_source, &Source::reachableChanged, this, nullptr);
        m_source = source;
        connect(source, &Source::reachableChanged, this, [this, source] {
            if (source->reachable() && m_paused)
                resume();
        });
    }
    setProblem(QString());
    for (const QVariant &value : tracks) {
        if (m_queue.size() >= MaxQueue)
            break;
        const QVariantMap track = value.toMap();
        const QString trackId = id(track);
        // A download on a card that is not in the phone is saved again
        // where downloads go now.
        if (!safeId(trackId) || savedFileExists(source, trackId) || id(m_track) == trackId)
            continue;
        bool queued = false;
        for (const QVariantMap &waiting : m_queue)
            queued = queued || id(waiting) == trackId;
        if (!queued) {
            QVariantMap entry;
            entry.insert(QStringLiteral("id"), trackId);
            entry.insert(QStringLiteral("title"), track.value(QStringLiteral("title")));
            m_queue.append(entry);
        }
    }
    saveQueue();
    emit progressChanged();
    resume();
}

void Downloads::downloadAll()
{
    Source *source = m_sources->active();
    if (source && source->canDownload())
        download(source->library()->missingTracks());
}

void Downloads::resume()
{
    m_retry.stop();
    m_paused = false;
    emit runningChanged();
    startNext();
}

void Downloads::pause(const QString &problem)
{
    m_paused = true;
    setProblem(problem);
    m_retry.start(RetryMs);
    emit runningChanged();
    emit progressChanged();
}

void Downloads::startNext()
{
    if (running() || m_paused)
        return;
    if (m_queue.isEmpty() || !m_source) {
        m_track.clear();
        saveQueue();
        emit progressChanged();
        emit runningChanged();
        return;
    }
    m_track = m_queue.takeFirst();
    m_directory = m_location.directoryFor(m_source->id(), m_source->downloadDirectory());
    if (m_directory.isEmpty()) {
        m_queue.prepend(m_track);
        m_track.clear();
        pause(tr("Downloads paused: the memory card is not in the phone"));
        return;
    }
    QDir().mkpath(m_directory);
    // The card's file system keeps no owners; this is for the phone.
    QFile::setPermissions(m_directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    if (!enoughSpace(m_directory, 0)) {
        m_queue.prepend(m_track);
        m_track.clear();
        pause(spaceProblem());
        return;
    }
    m_file.setFileName(partPath(m_track));
    // A part file from an interrupted download continues where it ended.
    m_offset = m_file.exists() ? m_file.size() : 0;
    if (!m_file.open(m_offset > 0 ? QIODevice::Append : QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_queue.prepend(m_track);
        m_track.clear();
        pause(tr("Downloads paused: the file could not be written"));
        return;
    }
    QNetworkRequest request = m_source->downloadRequest(m_track);
    if (m_offset > 0)
        request.setRawHeader("Range", "bytes=" + QByteArray::number(m_offset) + '-');
    m_expected = -1;
    m_received = 0;
    m_aborted = false;
    m_abortProblem.clear();
    m_finalPath.clear();
    m_reply = m_network.get(request);

    connect(m_reply, &QNetworkReply::metaDataChanged, this, [this] {
        const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const qint64 length = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        const QString type = m_reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
        // Removing the server deletes the source while its download runs.
        if (!m_source) {
            abortReply(Outcome::Skip, QString());
            return;
        }
        // The body of any other answer is not read, so it is not received
        // either.
        if (status != 200 && status != 206) {
            if (status == 416)
                abortReply(Outcome::Restart, QString());
            else if (status == 401 || status == 403)
                abortReply(Outcome::Pause, tr("Downloads paused: log in again"));
            else if ((status >= 300 && status < 400) || status == 502 || status == 503 || status == 504)
                abortReply(Outcome::Pause, tr("Downloads paused until the server is reachable again"));
            else
                abortReply(Outcome::Skip, tr("A track could not be downloaded and was skipped"));
            return;
        }
        // A page instead of music, as from a captive portal in a hotel
        // WLAN, must not be stored as the track.
        if (type.startsWith(QLatin1String("text/")) || type.contains(QLatin1String("json"))
            || type.contains(QLatin1String("xml"))) {
            abortReply(Outcome::Pause, tr("Downloads paused until the server is reachable again"));
            return;
        }
        if (status == 200 && m_offset > 0) {
            // The server sends the whole file instead of the rest.
            m_file.resize(0);
            m_offset = 0;
        }
        m_expected = length > 0 ? m_offset + length : -1;
        if (m_expected > MaxFileBytes) {
            abortReply(Outcome::Skip, tr("A track larger than 2 GB was skipped"));
            return;
        }
        if (m_expected > 0 && !enoughSpace(m_directory, m_expected - m_offset)) {
            abortReply(Outcome::Pause, spaceProblem());
            return;
        }
        m_finalPath = m_directory + QLatin1Char('/') + id(m_track) + QLatin1Char('.')
            + extensionFor(m_reply->header(QNetworkRequest::ContentTypeHeader).toString());
    });
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_finalPath.isEmpty())
            return;
        const QByteArray data = m_reply->readAll();
        m_received += data.size();
        if (m_offset + m_received > MaxFileBytes || m_file.write(data) != data.size()) {
            abortReply(Outcome::Pause, tr("Downloads paused: the file could not be written"));
            return;
        }
        emit progressChanged();
    });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply;
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError error = reply->error();
        const qint64 size = m_offset + m_received;
        if (m_aborted) {
            finish(m_abortOutcome, m_abortProblem);
        } else if (!m_source) {
            finish(Outcome::Skip, QString());
        } else if (error == QNetworkReply::NoError && !m_finalPath.isEmpty()
                   && (m_expected < 0 || size == m_expected)) {
            finish(Outcome::Done, QString());
        } else if (status == 416) {
            // The part file does not fit what the server has now.
            finish(Outcome::Restart, QString());
        } else if (status == 401 || status == 403) {
            finish(Outcome::Pause, tr("Downloads paused: log in again"));
        } else if (connectionLost(error) || (error == QNetworkReply::NoError && size < m_expected)) {
            finish(Outcome::Pause, tr("Downloads paused until the server is reachable again"));
        } else {
            finish(Outcome::Skip, tr("A track could not be downloaded and was skipped"));
        }
    });
    saveQueue();
    emit runningChanged();
    emit progressChanged();
}

void Downloads::abortReply(Outcome outcome, const QString &problem)
{
    m_aborted = true;
    m_abortOutcome = outcome;
    m_abortProblem = problem;
    m_reply->abort();
}

void Downloads::finish(Outcome outcome, const QString &problem)
{
    m_file.close();
    const QString part = m_file.fileName();
    m_reply = nullptr;
    switch (outcome) {
    case Outcome::Done: {
        QFile::remove(m_finalPath);
        const bool stored = m_source && QFile::rename(part, m_finalPath)
            && m_source->library()->markDownloaded(id(m_track), m_finalPath, QFileInfo(m_finalPath).size());
        if (!stored)
            QFile::remove(part);
        else
            m_source->keepCovers();
        setProblem(QString());
        bumpRevision();
        break;
    }
    case Outcome::Pause:
        // The part file stays; the track waits at the front.
        m_queue.prepend(m_track);
        m_track.clear();
        pause(problem);
        return;
    case Outcome::Restart:
        QFile::remove(part);
        m_queue.prepend(m_track);
        break;
    case Outcome::Skip:
        QFile::remove(part);
        setProblem(problem);
        break;
    }
    m_track.clear();
    startNext();
}

void Downloads::remove(const QVariantList &tracks)
{
    Source *source = m_sources->active();
    if (!source)
        return;
    for (const QVariant &value : tracks) {
        const QString trackId = id(value.toMap());
        for (int i = m_queue.size() - 1; i >= 0; --i) {
            if (id(m_queue.at(i)) == trackId)
                m_queue.removeAt(i);
        }
        if (source == m_source && id(m_track) == trackId && m_reply) {
            disconnect(m_reply, nullptr, this, nullptr);
            m_reply->abort();
            m_reply = nullptr;
            m_file.close();
            m_track.clear();
        }
        removeParts(source, trackId);
        const QString file = source->library()->downloadedFile(trackId);
        if (!file.isEmpty()) {
            QFile::remove(file);
            source->library()->forgetDownload(trackId);
        }
    }
    source->keepCovers();
    saveQueue();
    bumpRevision();
    emit progressChanged();
    startNext();
}

QVariantMap Downloads::storage() const
{
    qint64 used = 0;
    for (Source *source : m_sources->list())
        used += source->library()->downloadedBytes();
    QVariantMap result;
    result.insert(QStringLiteral("used"), used);
    result.insert(QStringLiteral("free"),
                  QStorageInfo(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).bytesAvailable());
    const QString card = m_location.cardRoot();
    result.insert(QStringLiteral("cardFree"), card.isEmpty() ? qint64(-1) : QStorageInfo(card).bytesAvailable());
    return result;
}

double Downloads::sourceBytes(const QString &sourceId) const
{
    Source *source = m_sources->source(sourceId);
    return source ? double(source->library()->downloadedBytes()) : 0;
}

void Downloads::removeAll()
{
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply = nullptr;
        m_file.close();
    }
    m_queue.clear();
    m_track.clear();
    m_paused = false;
    m_retry.stop();
    for (Source *source : m_sources->list()) {
        if (!source->canDownload())
            continue;
        QDir(source->downloadDirectory()).removeRecursively();
        const QString card = m_location.cardDirectoryFor(source->id());
        if (!card.isEmpty())
            QDir(card).removeRecursively();
        source->library()->forgetAllDownloads();
    }
    setProblem(QString());
    saveQueue();
    bumpRevision();
    emit runningChanged();
    emit progressChanged();
}

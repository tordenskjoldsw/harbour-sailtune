#include "downloads.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStorageInfo>

#include "../library/librarydatabase.h"
#include "../sources/source.h"
#include "../sources/sources.h"

namespace {

// Downloads stop before the phone's free space falls below this
// (PLAN.md section 14).
const qint64 ReserveBytes = qint64(2) * 1024 * 1024 * 1024;
// Far above a high-resolution FLAC track; a larger answer is not music.
const qint64 MaxFileBytes = qint64(2) * 1024 * 1024 * 1024;
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

bool enoughSpace(const QString &directory, qint64 needed)
{
    const QStorageInfo storage(directory);
    return storage.isValid() && storage.bytesAvailable() - qMax<qint64>(0, needed) >= ReserveBytes;
}

} // namespace

Downloads::Downloads(Sources *sources, QObject *parent)
    : QObject(parent)
    , m_sources(sources)
{
}

Downloads::~Downloads()
{
    cancel();
}

bool Downloads::running() const
{
    return !m_reply.isNull();
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
    return m_expected > 0 ? int(m_received * 100 / m_expected) : 0;
}

QString Downloads::problem() const
{
    return m_problem;
}

int Downloads::revision() const
{
    return m_revision;
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

void Downloads::download(const QVariantList &tracks)
{
    Source *source = m_sources->active();
    if (!source || !source->canDownload())
        return;
    if (m_source && m_source != source && (running() || !m_queue.isEmpty())) {
        setProblem(tr("Wait for the downloads of the other source to finish"));
        return;
    }
    m_source = source;
    setProblem(QString());
    for (const QVariant &value : tracks) {
        const QVariantMap track = value.toMap();
        const QString trackId = id(track);
        if (!safeId(trackId) || !source->library()->downloadedFile(trackId).isEmpty()
            || id(m_track) == trackId)
            continue;
        bool queued = false;
        for (const QVariantMap &waiting : m_queue)
            queued = queued || id(waiting) == trackId;
        if (!queued)
            m_queue.append(track);
    }
    emit progressChanged();
    startNext();
}

void Downloads::startNext()
{
    if (running())
        return;
    if (m_queue.isEmpty() || !m_source) {
        m_track.clear();
        emit progressChanged();
        emit runningChanged();
        return;
    }
    m_track = m_queue.takeFirst();
    const QString directory = m_source->downloadDirectory();
    QDir().mkpath(directory);
    QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    if (!enoughSpace(directory, 0)) {
        m_queue.clear();
        finishCurrent(false, tr("Downloads stopped: less than 2 GB free on the phone"));
        return;
    }
    m_file.setFileName(directory + QLatin1Char('/') + id(m_track) + PartSuffix);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_queue.clear();
        finishCurrent(false, tr("Downloads stopped: the file could not be written"));
        return;
    }
    m_expected = -1;
    m_received = 0;
    m_reply = m_network.get(m_source->downloadRequest(m_track));
    connect(m_reply, &QNetworkReply::metaDataChanged, this, [this] {
        m_expected = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        if (m_expected > MaxFileBytes) {
            m_queue.clear();
            m_reply->abort();
            return;
        }
        if (m_expected > 0 && !enoughSpace(m_source->downloadDirectory(), m_expected)) {
            m_queue.clear();
            setProblem(tr("Downloads stopped: less than 2 GB free on the phone"));
            m_reply->abort();
            return;
        }
        m_finalPath = m_source->downloadDirectory() + QLatin1Char('/') + id(m_track) + QLatin1Char('.')
            + extensionFor(m_reply->header(QNetworkRequest::ContentTypeHeader).toString());
    });
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        const QByteArray data = m_reply->readAll();
        m_received += data.size();
        if (m_received > MaxFileBytes || m_file.write(data) != data.size()) {
            m_queue.clear();
            m_reply->abort();
            return;
        }
        emit progressChanged();
    });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply;
        reply->deleteLater();
        const bool complete = reply->error() == QNetworkReply::NoError && !m_finalPath.isEmpty()
            && (m_expected < 0 || m_received == m_expected);
        if (!complete) {
            const bool cancelled = reply->error() == QNetworkReply::OperationCanceledError;
            if (!cancelled)
                m_queue.clear();
            finishCurrent(false, cancelled ? m_problem
                                           : tr("Downloads stopped: the server cannot be reached"));
            return;
        }
        finishCurrent(true, QString());
    });
    emit runningChanged();
    emit progressChanged();
}

void Downloads::finishCurrent(bool ok, const QString &problem)
{
    m_file.close();
    const QString partPath = m_file.fileName();
    if (ok && m_source) {
        QFile::remove(m_finalPath);
        ok = QFile::rename(partPath, m_finalPath)
            && m_source->library()->markDownloaded(id(m_track), m_finalPath, QFileInfo(m_finalPath).size());
    }
    if (!ok)
        QFile::remove(partPath);
    if (ok)
        bumpRevision();
    else if (!problem.isEmpty())
        setProblem(problem);
    m_reply = nullptr;
    m_finalPath.clear();
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
        const QString file = source->library()->downloadedFile(trackId);
        if (file.isEmpty())
            continue;
        QFile::remove(file);
        source->library()->forgetDownload(trackId);
    }
    bumpRevision();
}

void Downloads::cancel()
{
    m_queue.clear();
    if (m_reply)
        m_reply->abort();
    emit progressChanged();
}

#include "source.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

Source::Source(const QString &id, const QString &directory, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_directory(directory)
{
    QDir().mkpath(directory);
    QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    m_library.reset(new LibraryDatabase(directory + QStringLiteral("/library.sqlite")));
}

Source::~Source() = default;

QString Source::id() const
{
    return m_id;
}

bool Source::refreshing() const
{
    return m_refreshing;
}

QString Source::problem() const
{
    return m_problem;
}

bool Source::reachable() const
{
    return m_reachable;
}

void Source::setReachable(bool reachable)
{
    if (m_reachable == reachable)
        return;
    m_reachable = reachable;
    emit reachableChanged();
}

QVariantMap Source::counts(bool downloadedOnly) const
{
    return m_library->counts(downloadedOnly);
}

int Source::artistCount() const
{
    return m_library->artistCount();
}

int Source::albumCount() const
{
    return m_library->albumCount();
}

int Source::trackCount() const
{
    return m_library->trackCount();
}

int Source::playlistCount() const
{
    return m_library->playlistCount();
}

QUrl Source::playableUrl(const QVariantMap &track) const
{
    const QString file = m_library->downloadedFile(track.value(QStringLiteral("id")).toString());
    if (!file.isEmpty() && QFileInfo(file).isFile())
        return QUrl::fromLocalFile(file);
    return trackUrl(track);
}

bool Source::canDownload() const
{
    return false;
}

QNetworkRequest Source::downloadRequest(const QVariantMap &) const
{
    return QNetworkRequest();
}

QString Source::downloadDirectory() const
{
    return m_directory + QStringLiteral("/downloads");
}

LibraryDatabase *Source::library() const
{
    return m_library.data();
}

QString Source::directory() const
{
    return m_directory;
}

void Source::setRefreshing(bool refreshing)
{
    if (m_refreshing == refreshing)
        return;
    m_refreshing = refreshing;
    emit refreshingChanged();
}

void Source::setProblem(const QString &problem)
{
    if (m_problem == problem)
        return;
    m_problem = problem;
    emit problemChanged();
}

void Source::store(const LibrarySnapshot &snapshot)
{
    if (!m_library->replace(snapshot)) {
        setProblem(tr("The library could not be stored"));
        return;
    }
    emit libraryChanged();
}

#include "source.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

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

bool Source::isFavorite(const QString &trackId) const
{
    return m_library->isFavorite(trackId);
}

void Source::setFavorite(const QString &trackId, bool favorite)
{
    if (trackId.isEmpty() || !m_library->setFavorite(trackId, favorite))
        return;
    sendFavorite(trackId, favorite);
    emit favoritesChanged();
}

void Source::sendFavorite(const QString &, bool)
{
}

void Source::reportPlayed(const QString &)
{
}

bool Source::canEditPlaylists() const
{
    return false;
}

void Source::addToPlaylist(const QString &, const QStringList &)
{
    emit playlistEditFinished(tr("Playlists of this source cannot be changed"));
}

void Source::createPlaylist(const QString &, const QStringList &)
{
    emit playlistEditFinished(tr("Playlists of this source cannot be changed"));
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

void Source::keepCovers()
{
}

QString Source::coverFile(const QString &image) const
{
    // "<item id>|<tag>"; both are plain hex from the server.
    QString name = image;
    name.replace(QLatin1Char('|'), QLatin1Char('-'));
    if (name.isEmpty() || name.size() > 128)
        return QString();
    for (const QChar c : name) {
        if (!(c.isLetterOrNumber() && c.unicode() < 128) && c != QLatin1Char('-'))
            return QString();
    }
    return downloadDirectory() + QStringLiteral("/covers/") + name + QStringLiteral(".jpg");
}

void Source::pruneCovers(const QStringList &images) const
{
    QSet<QString> keep;
    for (const QString &image : images)
        keep.insert(QFileInfo(coverFile(image)).fileName());
    QDir covers(downloadDirectory() + QStringLiteral("/covers"));
    const QStringList files = covers.entryList(QDir::Files);
    for (const QString &file : files) {
        if (!keep.contains(file))
            covers.remove(file);
    }
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

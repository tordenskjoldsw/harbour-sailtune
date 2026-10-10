#include "source.h"

#include <QDir>
#include <QFile>

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

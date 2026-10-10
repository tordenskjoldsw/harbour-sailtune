#include "librarydatabase.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>
#include <QVariantMap>

namespace {

// Search terms are matched as plain substrings; the wildcards of LIKE are
// escaped so a "%" in a title is just a character.
QString likePattern(const QString &term)
{
    QString escaped = term.trimmed();
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    escaped.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    return QLatin1Char('%') + escaped + QLatin1Char('%');
}

QString sortKey(const QString &text)
{
    return text.trimmed().toCaseFolded();
}

const int SearchLimit = 50;

const char *const Schema[] = {
    "CREATE TABLE IF NOT EXISTS artists (id TEXT PRIMARY KEY, name TEXT NOT NULL,"
    " sort_name TEXT NOT NULL, image TEXT NOT NULL)",
    "CREATE TABLE IF NOT EXISTS albums (id TEXT PRIMARY KEY, title TEXT NOT NULL,"
    " sort_title TEXT NOT NULL, artist_id TEXT NOT NULL, artist TEXT NOT NULL,"
    " year INTEGER NOT NULL, image TEXT NOT NULL)",
    "CREATE TABLE IF NOT EXISTS tracks (id TEXT PRIMARY KEY, title TEXT NOT NULL,"
    " album_id TEXT NOT NULL, album TEXT NOT NULL, artist TEXT NOT NULL,"
    " disc INTEGER NOT NULL, number INTEGER NOT NULL, seconds INTEGER NOT NULL,"
    " location TEXT NOT NULL)",
    "CREATE TABLE IF NOT EXISTS playlists (id TEXT PRIMARY KEY, name TEXT NOT NULL,"
    " sort_name TEXT NOT NULL, image TEXT NOT NULL)",
    "CREATE TABLE IF NOT EXISTS playlist_tracks (playlist_id TEXT NOT NULL,"
    " position INTEGER NOT NULL, track_id TEXT NOT NULL,"
    " PRIMARY KEY (playlist_id, position))",
    "CREATE INDEX IF NOT EXISTS albums_by_artist ON albums (artist_id)",
    "CREATE INDEX IF NOT EXISTS tracks_by_album ON tracks (album_id)",
};

const char TrackColumns[] =
    "t.id AS id, t.title AS title, t.album_id AS albumId, t.album AS album,"
    " t.artist AS artist, t.disc AS disc, t.number AS number, t.seconds AS seconds,"
    " t.location AS location, COALESCE(a.image, '') AS image";

} // namespace

LibraryDatabase::LibraryDatabase(const QString &path)
    : m_connection(QStringLiteral("library-") + QUuid::createUuid().toString())
{
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    database.setDatabaseName(path);
    if (!database.open())
        return;
    QSqlQuery query(database);
    for (const char *statement : Schema) {
        if (!query.exec(QLatin1String(statement)))
            return;
    }
    m_open = true;
}

LibraryDatabase::~LibraryDatabase()
{
    {
        QSqlDatabase database = QSqlDatabase::database(m_connection, false);
        database.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
}

bool LibraryDatabase::isOpen() const
{
    return m_open;
}

bool LibraryDatabase::replace(const LibrarySnapshot &snapshot)
{
    if (!m_open)
        return false;
    QSqlDatabase database = QSqlDatabase::database(m_connection);
    if (!database.transaction())
        return false;
    QSqlQuery query(database);
    bool ok = query.exec(QStringLiteral("DELETE FROM artists"))
        && query.exec(QStringLiteral("DELETE FROM albums"))
        && query.exec(QStringLiteral("DELETE FROM tracks"))
        && query.exec(QStringLiteral("DELETE FROM playlists"))
        && query.exec(QStringLiteral("DELETE FROM playlist_tracks"));

    ok = ok && query.prepare(QStringLiteral("INSERT OR REPLACE INTO artists VALUES (?, ?, ?, ?)"));
    for (int i = 0; ok && i < snapshot.artists.size(); ++i) {
        const LibraryArtist &artist = snapshot.artists.at(i);
        query.addBindValue(artist.id);
        query.addBindValue(artist.name);
        query.addBindValue(sortKey(artist.name));
        query.addBindValue(artist.image);
        ok = query.exec();
    }
    ok = ok && query.prepare(QStringLiteral("INSERT OR REPLACE INTO albums VALUES (?, ?, ?, ?, ?, ?, ?)"));
    for (int i = 0; ok && i < snapshot.albums.size(); ++i) {
        const LibraryAlbum &album = snapshot.albums.at(i);
        query.addBindValue(album.id);
        query.addBindValue(album.title);
        query.addBindValue(sortKey(album.title));
        query.addBindValue(album.artistId);
        query.addBindValue(album.artist);
        query.addBindValue(album.year);
        query.addBindValue(album.image);
        ok = query.exec();
    }
    ok = ok && query.prepare(QStringLiteral("INSERT OR REPLACE INTO tracks VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    for (int i = 0; ok && i < snapshot.tracks.size(); ++i) {
        const LibraryTrack &track = snapshot.tracks.at(i);
        query.addBindValue(track.id);
        query.addBindValue(track.title);
        query.addBindValue(track.albumId);
        query.addBindValue(track.album);
        query.addBindValue(track.artist);
        query.addBindValue(track.disc);
        query.addBindValue(track.number);
        query.addBindValue(track.seconds);
        query.addBindValue(track.location);
        ok = query.exec();
    }
    for (int i = 0; ok && i < snapshot.playlists.size(); ++i) {
        const LibraryPlaylist &playlist = snapshot.playlists.at(i);
        ok = query.prepare(QStringLiteral("INSERT OR REPLACE INTO playlists VALUES (?, ?, ?, ?)"));
        query.addBindValue(playlist.id);
        query.addBindValue(playlist.name);
        query.addBindValue(sortKey(playlist.name));
        query.addBindValue(playlist.image);
        ok = ok && query.exec();
        ok = ok && query.prepare(QStringLiteral("INSERT INTO playlist_tracks VALUES (?, ?, ?)"));
        for (int position = 0; ok && position < playlist.trackIds.size(); ++position) {
            query.addBindValue(playlist.id);
            query.addBindValue(position);
            query.addBindValue(playlist.trackIds.at(position));
            ok = query.exec();
        }
    }
    if (!ok) {
        database.rollback();
        return false;
    }
    return database.commit();
}

int LibraryDatabase::count(const char *table) const
{
    if (!m_open)
        return 0;
    QSqlQuery query(QSqlDatabase::database(m_connection));
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM ") + QLatin1String(table)) || !query.next())
        return 0;
    return query.value(0).toInt();
}

int LibraryDatabase::artistCount() const
{
    return count("artists");
}

int LibraryDatabase::albumCount() const
{
    return count("albums");
}

int LibraryDatabase::trackCount() const
{
    return count("tracks");
}

int LibraryDatabase::playlistCount() const
{
    return count("playlists");
}

QVariantList LibraryDatabase::select(const QString &sql, const QVariantList &values) const
{
    QVariantList rows;
    if (!m_open)
        return rows;
    QSqlQuery query(QSqlDatabase::database(m_connection));
    query.setForwardOnly(true);
    if (!query.prepare(sql))
        return rows;
    for (const QVariant &value : values)
        query.addBindValue(value);
    if (!query.exec())
        return rows;
    while (query.next()) {
        const QSqlRecord record = query.record();
        QVariantMap row;
        for (int i = 0; i < record.count(); ++i)
            row.insert(record.fieldName(i), record.value(i));
        rows.append(row);
    }
    return rows;
}

QVariantList LibraryDatabase::artists() const
{
    return select(QStringLiteral(
        "SELECT ar.id AS id, ar.name AS title, COUNT(al.id) AS count, ar.image AS image"
        " FROM artists ar LEFT JOIN albums al ON al.artist_id = ar.id"
        " GROUP BY ar.id ORDER BY ar.sort_name"), {});
}

QVariantList LibraryDatabase::albums(const QString &artistId) const
{
    const QString columns = QStringLiteral(
        "SELECT id, title, artist, year, image, artist_id AS artistId FROM albums");
    if (artistId.isEmpty())
        return select(columns + QStringLiteral(" ORDER BY sort_title"), {});
    return select(columns + QStringLiteral(" WHERE artist_id = ? ORDER BY year, sort_title"),
                  {artistId});
}

QVariantList LibraryDatabase::tracksOfAlbum(const QString &albumId) const
{
    return select(QStringLiteral("SELECT ") + QLatin1String(TrackColumns)
                      + QStringLiteral(" FROM tracks t LEFT JOIN albums a ON a.id = t.album_id"
                                       " WHERE t.album_id = ? ORDER BY t.disc, t.number, t.title"),
                  {albumId});
}

QVariantList LibraryDatabase::playlists() const
{
    return select(QStringLiteral(
        "SELECT p.id AS id, p.name AS title, COUNT(pt.track_id) AS count, p.image AS image"
        " FROM playlists p LEFT JOIN playlist_tracks pt ON pt.playlist_id = p.id"
        " GROUP BY p.id ORDER BY p.sort_name"), {});
}

QVariantList LibraryDatabase::tracksOfPlaylist(const QString &playlistId) const
{
    return select(QStringLiteral("SELECT ") + QLatin1String(TrackColumns)
                      + QStringLiteral(" FROM playlist_tracks pt JOIN tracks t ON t.id = pt.track_id"
                                       " LEFT JOIN albums a ON a.id = t.album_id"
                                       " WHERE pt.playlist_id = ? ORDER BY pt.position"),
                  {playlistId});
}

QVariantList LibraryDatabase::searchArtists(const QString &term) const
{
    return select(QStringLiteral("SELECT id, name AS title, image FROM artists"
                                 " WHERE name LIKE ? ESCAPE '\\' ORDER BY sort_name LIMIT %1")
                      .arg(SearchLimit),
                  {likePattern(term)});
}

QVariantList LibraryDatabase::searchAlbums(const QString &term) const
{
    return select(QStringLiteral("SELECT id, title, artist, year, image, artist_id AS artistId"
                                 " FROM albums WHERE title LIKE ? ESCAPE '\\'"
                                 " ORDER BY sort_title LIMIT %1")
                      .arg(SearchLimit),
                  {likePattern(term)});
}

QVariantList LibraryDatabase::searchTracks(const QString &term) const
{
    return select(QStringLiteral("SELECT ") + QLatin1String(TrackColumns)
                      + QStringLiteral(" FROM tracks t LEFT JOIN albums a ON a.id = t.album_id"
                                       " WHERE t.title LIKE ? ESCAPE '\\'"
                                       " ORDER BY t.title LIMIT %1")
                            .arg(SearchLimit),
                  {likePattern(term)});
}

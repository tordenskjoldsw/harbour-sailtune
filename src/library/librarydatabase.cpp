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

// A null QString binds as SQL NULL, which the NOT NULL columns refuse; a
// server leaves out fields such as an album's artist or image.
QString notNull(const QString &text)
{
    return text.isNull() ? QStringLiteral("") : text;
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
    "CREATE TABLE IF NOT EXISTS downloads (track_id TEXT PRIMARY KEY, file TEXT NOT NULL,"
    " bytes INTEGER NOT NULL)",
    "CREATE TABLE IF NOT EXISTS favorites (track_id TEXT PRIMARY KEY)",
    "CREATE INDEX IF NOT EXISTS albums_by_artist ON albums (artist_id)",
    "CREATE INDEX IF NOT EXISTS tracks_by_album ON tracks (album_id)",
};

// Per connection: the path prefixes under which downloads can be read
// now, and the downloads below them. A download on a memory card that is
// not in the phone counts as not downloaded until the card is back; with
// no prefixes set, every download counts.
const char *const TemporarySchema[] = {
    "CREATE TEMP TABLE IF NOT EXISTS present_roots (root TEXT NOT NULL)",
    "CREATE TEMP VIEW IF NOT EXISTS present_downloads AS SELECT d.* FROM downloads d"
    " WHERE NOT EXISTS (SELECT 1 FROM present_roots)"
    " OR EXISTS (SELECT 1 FROM present_roots r WHERE substr(d.file, 1, length(r.root)) = r.root)",
};

// Every track query joins the album as "a" and the present downloads as
// "d".
const char TrackColumns[] =
    "t.id AS id, t.title AS title, t.album_id AS albumId, t.album AS album,"
    " t.artist AS artist, t.disc AS disc, t.number AS number, t.seconds AS seconds,"
    " t.location AS location, COALESCE(a.image, '') AS image,"
    " d.track_id IS NOT NULL AS downloaded,"
    " EXISTS (SELECT 1 FROM favorites f WHERE f.track_id = t.id) AS favorite";

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
    for (const char *statement : TemporarySchema) {
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
    if (ok && snapshot.favoritesFromSource) {
        ok = query.exec(QStringLiteral("DELETE FROM favorites"))
            && query.prepare(QStringLiteral("INSERT OR REPLACE INTO favorites VALUES (?)"));
        for (int i = 0; ok && i < snapshot.favoriteTrackIds.size(); ++i) {
            query.addBindValue(notNull(snapshot.favoriteTrackIds.at(i)));
            ok = query.exec();
        }
    }

    ok = ok && query.prepare(QStringLiteral("INSERT OR REPLACE INTO artists VALUES (?, ?, ?, ?)"));
    for (int i = 0; ok && i < snapshot.artists.size(); ++i) {
        const LibraryArtist &artist = snapshot.artists.at(i);
        query.addBindValue(notNull(artist.id));
        query.addBindValue(notNull(artist.name));
        query.addBindValue(notNull(sortKey(artist.name)));
        query.addBindValue(notNull(artist.image));
        ok = query.exec();
    }
    ok = ok && query.prepare(QStringLiteral("INSERT OR REPLACE INTO albums VALUES (?, ?, ?, ?, ?, ?, ?)"));
    for (int i = 0; ok && i < snapshot.albums.size(); ++i) {
        const LibraryAlbum &album = snapshot.albums.at(i);
        query.addBindValue(notNull(album.id));
        query.addBindValue(notNull(album.title));
        query.addBindValue(notNull(sortKey(album.title)));
        query.addBindValue(notNull(album.artistId));
        query.addBindValue(notNull(album.artist));
        query.addBindValue(album.year);
        query.addBindValue(notNull(album.image));
        ok = query.exec();
    }
    ok = ok && query.prepare(QStringLiteral("INSERT OR REPLACE INTO tracks VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    for (int i = 0; ok && i < snapshot.tracks.size(); ++i) {
        const LibraryTrack &track = snapshot.tracks.at(i);
        query.addBindValue(notNull(track.id));
        query.addBindValue(notNull(track.title));
        query.addBindValue(notNull(track.albumId));
        query.addBindValue(notNull(track.album));
        query.addBindValue(notNull(track.artist));
        query.addBindValue(track.disc);
        query.addBindValue(track.number);
        query.addBindValue(track.seconds);
        query.addBindValue(notNull(track.location));
        ok = query.exec();
    }
    for (int i = 0; ok && i < snapshot.playlists.size(); ++i) {
        const LibraryPlaylist &playlist = snapshot.playlists.at(i);
        ok = query.prepare(QStringLiteral("INSERT OR REPLACE INTO playlists VALUES (?, ?, ?, ?)"));
        query.addBindValue(notNull(playlist.id));
        query.addBindValue(notNull(playlist.name));
        query.addBindValue(notNull(sortKey(playlist.name)));
        query.addBindValue(notNull(playlist.image));
        ok = ok && query.exec();
        ok = ok && query.prepare(QStringLiteral("INSERT INTO playlist_tracks VALUES (?, ?, ?)"));
        for (int position = 0; ok && position < playlist.trackIds.size(); ++position) {
            query.addBindValue(notNull(playlist.id));
            query.addBindValue(position);
            query.addBindValue(notNull(playlist.trackIds.at(position)));
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

// An album or an artist counts as downloaded when one of its tracks is.
const char AlbumHasDownload[] =
    "EXISTS (SELECT 1 FROM tracks tx JOIN present_downloads dx ON dx.track_id = tx.id"
    " WHERE tx.album_id = al.id)";

QVariantList LibraryDatabase::artists(bool downloadedOnly) const
{
    return select(QStringLiteral(
                      "SELECT ar.id AS id, ar.name AS title, COUNT(al.id) AS count, ar.image AS image"
                      " FROM artists ar LEFT JOIN albums al ON al.artist_id = ar.id%1"
                      " GROUP BY ar.id%2 ORDER BY ar.sort_name")
                      .arg(downloadedOnly ? QStringLiteral(" AND ") + QLatin1String(AlbumHasDownload) : QString(),
                           downloadedOnly ? QStringLiteral(" HAVING COUNT(al.id) > 0") : QString()),
                  {});
}

QVariantList LibraryDatabase::albums(const QString &artistId, bool downloadedOnly) const
{
    QString sql = QStringLiteral(
        "SELECT al.id AS id, al.title AS title, al.artist AS artist, al.year AS year,"
        " al.image AS image, al.artist_id AS artistId FROM albums al WHERE 1");
    QVariantList values;
    if (!artistId.isEmpty()) {
        sql += QStringLiteral(" AND al.artist_id = ?");
        values.append(artistId);
    }
    if (downloadedOnly)
        sql += QStringLiteral(" AND ") + QLatin1String(AlbumHasDownload);
    sql += artistId.isEmpty() ? QStringLiteral(" ORDER BY al.sort_title")
                              : QStringLiteral(" ORDER BY al.year, al.sort_title");
    return select(sql, values);
}

QVariantList LibraryDatabase::tracksOfAlbum(const QString &albumId, bool downloadedOnly) const
{
    return select(QStringLiteral("SELECT ") + QLatin1String(TrackColumns)
                      + QStringLiteral(" FROM tracks t LEFT JOIN albums a ON a.id = t.album_id"
                                       " LEFT JOIN present_downloads d ON d.track_id = t.id"
                                       " WHERE t.album_id = ?%1 ORDER BY t.disc, t.number, t.title")
                            .arg(downloadedOnly ? QStringLiteral(" AND d.track_id IS NOT NULL") : QString()),
                  {albumId});
}

QVariantList LibraryDatabase::playlists(bool downloadedOnly) const
{
    return select(QStringLiteral(
                      "SELECT p.id AS id, p.name AS title, COUNT(pt.track_id) AS count, p.image AS image"
                      " FROM playlists p LEFT JOIN playlist_tracks pt ON pt.playlist_id = p.id%1"
                      " GROUP BY p.id%2 ORDER BY p.sort_name")
                      .arg(downloadedOnly ? QStringLiteral(" AND pt.track_id IN (SELECT track_id FROM present_downloads)")
                                          : QString(),
                           downloadedOnly ? QStringLiteral(" HAVING COUNT(pt.track_id) > 0") : QString()),
                  {});
}

QVariantList LibraryDatabase::tracksOfPlaylist(const QString &playlistId, bool downloadedOnly) const
{
    return select(QStringLiteral("SELECT ") + QLatin1String(TrackColumns)
                      + QStringLiteral(" FROM playlist_tracks pt JOIN tracks t ON t.id = pt.track_id"
                                       " LEFT JOIN albums a ON a.id = t.album_id"
                                       " LEFT JOIN present_downloads d ON d.track_id = t.id"
                                       " WHERE pt.playlist_id = ?%1 ORDER BY pt.position")
                            .arg(downloadedOnly ? QStringLiteral(" AND d.track_id IS NOT NULL") : QString()),
                  {playlistId});
}

QVariantList LibraryDatabase::favoriteTracks(bool downloadedOnly) const
{
    return select(QStringLiteral("SELECT ") + QLatin1String(TrackColumns)
                      + QStringLiteral(" FROM favorites fav JOIN tracks t ON t.id = fav.track_id"
                                       " LEFT JOIN albums a ON a.id = t.album_id"
                                       " LEFT JOIN present_downloads d ON d.track_id = t.id"
                                       " WHERE 1%1 ORDER BY t.artist, t.album, t.disc, t.number")
                            .arg(downloadedOnly ? QStringLiteral(" AND d.track_id IS NOT NULL") : QString()),
                  {});
}

bool LibraryDatabase::setFavorite(const QString &trackId, bool favorite)
{
    if (!m_open)
        return false;
    QSqlQuery query(QSqlDatabase::database(m_connection));
    if (!query.prepare(favorite ? QStringLiteral("INSERT OR REPLACE INTO favorites VALUES (?)")
                                : QStringLiteral("DELETE FROM favorites WHERE track_id = ?")))
        return false;
    query.addBindValue(notNull(trackId));
    return query.exec();
}

bool LibraryDatabase::appendToPlaylist(const QString &playlistId, const QString &name,
                                       const QStringList &trackIds)
{
    if (!m_open)
        return false;
    QSqlDatabase database = QSqlDatabase::database(m_connection);
    if (!database.transaction())
        return false;
    QSqlQuery query(database);
    bool ok = query.prepare(QStringLiteral("INSERT OR IGNORE INTO playlists VALUES (?, ?, ?, '')"));
    query.addBindValue(notNull(playlistId));
    query.addBindValue(notNull(name));
    query.addBindValue(notNull(sortKey(name)));
    ok = ok && query.exec()
        && query.prepare(QStringLiteral("SELECT COALESCE(MAX(position) + 1, 0) FROM playlist_tracks"
                                        " WHERE playlist_id = ?"));
    query.addBindValue(notNull(playlistId));
    ok = ok && query.exec() && query.next();
    int position = ok ? query.value(0).toInt() : 0;
    ok = ok && query.prepare(QStringLiteral("INSERT INTO playlist_tracks VALUES (?, ?, ?)"));
    for (int i = 0; ok && i < trackIds.size(); ++i) {
        query.addBindValue(notNull(playlistId));
        query.addBindValue(position++);
        query.addBindValue(notNull(trackIds.at(i)));
        ok = query.exec();
    }
    if (!ok) {
        database.rollback();
        return false;
    }
    return database.commit();
}

bool LibraryDatabase::isFavorite(const QString &trackId) const
{
    return !select(QStringLiteral("SELECT 1 AS yes FROM favorites WHERE track_id = ?"), {trackId}).isEmpty();
}

QVariantMap LibraryDatabase::counts(bool downloadedOnly) const
{
    QVariantMap counts;
    counts.insert(QStringLiteral("favorites"), favoriteTracks(downloadedOnly).size());
    counts.insert(QStringLiteral("artists"), artists(downloadedOnly).size());
    counts.insert(QStringLiteral("albums"), albums(QString(), downloadedOnly).size());
    counts.insert(QStringLiteral("playlists"), playlists(downloadedOnly).size());
    const QVariantList tracks = select(
        downloadedOnly ? QStringLiteral("SELECT COUNT(*) AS n FROM tracks t JOIN present_downloads d ON d.track_id = t.id")
                       : QStringLiteral("SELECT COUNT(*) AS n FROM tracks"),
        {});
    counts.insert(QStringLiteral("tracks"), tracks.isEmpty() ? 0 : tracks.first().toMap().value(QStringLiteral("n")));
    return counts;
}

QVariantList LibraryDatabase::tracksByIds(const QStringList &ids) const
{
    QVariantList tracks;
    for (const QString &id : ids) {
        const QVariantList found = select(QStringLiteral("SELECT ") + QLatin1String(TrackColumns)
                                              + QStringLiteral(" FROM tracks t LEFT JOIN albums a"
                                                               " ON a.id = t.album_id LEFT JOIN present_downloads d"
                                                               " ON d.track_id = t.id WHERE t.id = ?"),
                                          {id});
        if (!found.isEmpty())
            tracks.append(found.first());
    }
    return tracks;
}

bool LibraryDatabase::markDownloaded(const QString &trackId, const QString &file, qint64 bytes)
{
    if (!m_open)
        return false;
    QSqlQuery query(QSqlDatabase::database(m_connection));
    if (!query.prepare(QStringLiteral("INSERT OR REPLACE INTO downloads VALUES (?, ?, ?)")))
        return false;
    query.addBindValue(notNull(trackId));
    query.addBindValue(notNull(file));
    query.addBindValue(bytes);
    return query.exec();
}

bool LibraryDatabase::forgetDownload(const QString &trackId)
{
    if (!m_open)
        return false;
    QSqlQuery query(QSqlDatabase::database(m_connection));
    if (!query.prepare(QStringLiteral("DELETE FROM downloads WHERE track_id = ?")))
        return false;
    query.addBindValue(trackId);
    return query.exec();
}

QString LibraryDatabase::downloadedFile(const QString &trackId) const
{
    const QVariantList rows = select(QStringLiteral("SELECT file FROM downloads WHERE track_id = ?"),
                                     {trackId});
    return rows.isEmpty() ? QString() : rows.first().toMap().value(QStringLiteral("file")).toString();
}

bool LibraryDatabase::setPresentRoots(const QStringList &roots)
{
    if (!m_open)
        return false;
    QSqlDatabase database = QSqlDatabase::database(m_connection);
    if (!database.transaction())
        return false;
    QSqlQuery query(database);
    bool ok = query.exec(QStringLiteral("DELETE FROM present_roots"))
        && query.prepare(QStringLiteral("INSERT INTO present_roots VALUES (?)"));
    for (int i = 0; ok && i < roots.size(); ++i) {
        query.addBindValue(notNull(roots.at(i)));
        ok = query.exec();
    }
    if (!ok) {
        database.rollback();
        return false;
    }
    return database.commit();
}

bool LibraryDatabase::forgetAllDownloads()
{
    if (!m_open)
        return false;
    QSqlQuery query(QSqlDatabase::database(m_connection));
    return query.exec(QStringLiteral("DELETE FROM downloads"));
}

QStringList LibraryDatabase::downloadedImages() const
{
    const QVariantList rows = select(QStringLiteral(
        "SELECT al.image AS image FROM downloads d JOIN tracks t ON t.id = d.track_id"
        " JOIN albums al ON al.id = t.album_id"
        " UNION SELECT ar.image FROM downloads d JOIN tracks t ON t.id = d.track_id"
        " JOIN albums al ON al.id = t.album_id JOIN artists ar ON ar.id = al.artist_id"
        " UNION SELECT p.image FROM downloads d JOIN playlist_tracks pt ON pt.track_id = d.track_id"
        " JOIN playlists p ON p.id = pt.playlist_id"), {});
    QStringList images;
    for (const QVariant &row : rows) {
        const QString image = row.toMap().value(QStringLiteral("image")).toString();
        if (!image.isEmpty())
            images.append(image);
    }
    return images;
}

qint64 LibraryDatabase::downloadedBytes() const
{
    const QVariantList rows = select(QStringLiteral("SELECT COALESCE(SUM(bytes), 0) AS bytes FROM downloads"), {});
    return rows.isEmpty() ? 0 : rows.first().toMap().value(QStringLiteral("bytes")).toLongLong();
}

QVariantList LibraryDatabase::searchArtists(const QString &term, bool downloadedOnly) const
{
    return select(QStringLiteral("SELECT ar.id AS id, ar.name AS title, ar.image AS image FROM artists ar"
                                 " WHERE ar.name LIKE ? ESCAPE '\\'%1 ORDER BY ar.sort_name LIMIT %2")
                      .arg(downloadedOnly ? QStringLiteral(" AND EXISTS (SELECT 1 FROM albums al WHERE"
                                                           " al.artist_id = ar.id AND ")
                                                + QLatin1String(AlbumHasDownload) + QLatin1Char(')')
                                          : QString())
                      .arg(SearchLimit),
                  {likePattern(term)});
}

QVariantList LibraryDatabase::searchAlbums(const QString &term, bool downloadedOnly) const
{
    return select(QStringLiteral("SELECT al.id AS id, al.title AS title, al.artist AS artist, al.year AS year,"
                                 " al.image AS image, al.artist_id AS artistId FROM albums al"
                                 " WHERE al.title LIKE ? ESCAPE '\\'%1 ORDER BY al.sort_title LIMIT %2")
                      .arg(downloadedOnly ? QStringLiteral(" AND ") + QLatin1String(AlbumHasDownload) : QString())
                      .arg(SearchLimit),
                  {likePattern(term)});
}

QVariantList LibraryDatabase::searchTracks(const QString &term, bool downloadedOnly) const
{
    return select(QStringLiteral("SELECT ") + QLatin1String(TrackColumns)
                      + QStringLiteral(" FROM tracks t LEFT JOIN albums a ON a.id = t.album_id"
                                       " LEFT JOIN present_downloads d ON d.track_id = t.id"
                                       " WHERE t.title LIKE ? ESCAPE '\\'%1 ORDER BY t.title LIMIT %2")
                            .arg(downloadedOnly ? QStringLiteral(" AND d.track_id IS NOT NULL") : QString())
                            .arg(SearchLimit),
                  {likePattern(term)});
}

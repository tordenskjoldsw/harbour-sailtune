#ifndef LIBRARYDATABASE_H
#define LIBRARYDATABASE_H

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

// What a source knows about its music, in a form both the phone's media
// index and a server can fill. Ids are the source's own, kept as text.
struct LibraryArtist {
    QString id;
    QString name;
    QString image;
};

struct LibraryAlbum {
    QString id;
    QString title;
    QString artistId;
    QString artist;
    int year = 0;
    QString image;
};

struct LibraryTrack {
    QString id;
    QString title;
    QString albumId;
    QString album;
    QString artist;
    int disc = 0;
    int number = 0;
    int seconds = 0;
    // A file URL for the phone's music; empty for a server, which builds
    // its stream URL from the id.
    QString location;
};

struct LibraryPlaylist {
    QString id;
    QString name;
    QString image;
    QVector<QString> trackIds;
};

struct LibrarySnapshot {
    QVector<LibraryArtist> artists;
    QVector<LibraryAlbum> albums;
    QVector<LibraryTrack> tracks;
    QVector<LibraryPlaylist> playlists;
    // A server knows its user's favorites, and a refresh takes them over;
    // the phone's favorites live only in the cache and stay.
    bool favoritesFromSource = false;
    QVector<QString> favoriteTrackIds;
};

// The cache of one source in its own SQLite file. The UI reads only from
// here, so the library shows at once and also offline; a refresh
// replaces the whole content in one transaction.
class LibraryDatabase
{
public:
    explicit LibraryDatabase(const QString &path);
    ~LibraryDatabase();
    LibraryDatabase(const LibraryDatabase &) = delete;
    LibraryDatabase &operator=(const LibraryDatabase &) = delete;

    bool isOpen() const;
    bool replace(const LibrarySnapshot &snapshot);

    int artistCount() const;
    int albumCount() const;
    int trackCount() const;
    int playlistCount() const;

    // Each row is a map with the keys the list models expose.
    // With downloadedOnly, only what plays without a network: downloaded
    // tracks and the albums, artists and playlists that hold one.
    QVariantList artists(bool downloadedOnly) const;
    QVariantList albums(const QString &artistId, bool downloadedOnly) const;
    QVariantList tracksOfAlbum(const QString &albumId, bool downloadedOnly) const;
    QVariantList playlists(bool downloadedOnly) const;
    QVariantList tracksOfPlaylist(const QString &playlistId, bool downloadedOnly) const;
    QVariantList searchArtists(const QString &term, bool downloadedOnly) const;
    QVariantList searchAlbums(const QString &term, bool downloadedOnly) const;
    QVariantList searchTracks(const QString &term, bool downloadedOnly) const;
    QVariantList favoriteTracks(bool downloadedOnly) const;
    // "artists", "albums", "tracks", "playlists" and "favorites".
    QVariantMap counts(bool downloadedOnly) const;
    bool setFavorite(const QString &trackId, bool favorite);
    // After the server took the tracks: appends them to the playlist,
    // which is added with this name when the cache does not have it yet.
    bool appendToPlaylist(const QString &playlistId, const QString &name, const QStringList &trackIds);
    bool isFavorite(const QString &trackId) const;
    // The tracks with these ids in this order; ids no longer cached are
    // left out.
    QVariantList tracksByIds(const QStringList &ids) const;

    // Downloads outlive a refresh: their table is never replaced.
    bool markDownloaded(const QString &trackId, const QString &file, qint64 bytes);
    bool forgetDownload(const QString &trackId);
    // The downloaded file of a track, or empty.
    QString downloadedFile(const QString &trackId) const;
    qint64 downloadedBytes() const;
    bool forgetAllDownloads();
    // The images of the albums, artists and playlists that have downloaded
    // tracks: what must show without a network.
    QStringList downloadedImages() const;

private:
    int count(const char *table) const;
    QVariantList select(const QString &sql, const QVariantList &values) const;

    QString m_connection;
    bool m_open = false;
};

#endif // LIBRARYDATABASE_H

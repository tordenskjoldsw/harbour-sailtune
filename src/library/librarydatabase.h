#ifndef LIBRARYDATABASE_H
#define LIBRARYDATABASE_H

#include <QString>
#include <QVariantList>
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
    QVariantList artists() const;
    QVariantList albums(const QString &artistId) const;
    QVariantList tracksOfAlbum(const QString &albumId) const;
    QVariantList playlists() const;
    QVariantList tracksOfPlaylist(const QString &playlistId) const;
    QVariantList searchArtists(const QString &term) const;
    QVariantList searchAlbums(const QString &term) const;
    QVariantList searchTracks(const QString &term) const;

private:
    int count(const char *table) const;
    QVariantList select(const QString &sql, const QVariantList &values) const;

    QString m_connection;
    bool m_open = false;
};

#endif // LIBRARYDATABASE_H

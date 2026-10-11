#ifndef SOURCE_H
#define SOURCE_H

#include <QNetworkRequest>
#include <QObject>
#include <QScopedPointer>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include "../library/librarydatabase.h"

// Where music comes from: the phone itself or a server. Everything above
// this line (library pages, player, downloads) talks to a Source and
// never to a particular kind; a new server family is a new subclass.
class Source : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString kind READ kind CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)
    Q_PROPERTY(int artistCount READ artistCount NOTIFY libraryChanged)
    Q_PROPERTY(int albumCount READ albumCount NOTIFY libraryChanged)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY libraryChanged)
    Q_PROPERTY(int playlistCount READ playlistCount NOTIFY libraryChanged)
    Q_PROPERTY(bool canDownload READ canDownload CONSTANT)
    // Servers keep playlists; the phone's are read only.
    Q_PROPERTY(bool canEditPlaylists READ canEditPlaylists CONSTANT)
    // False after the last refresh found no connection to the server, as
    // away from home without the VPN.
    Q_PROPERTY(bool reachable READ reachable NOTIFY reachableChanged)

public:
    // directory: the source's own folder in the app's private storage.
    Source(const QString &id, const QString &directory, QObject *parent = nullptr);
    ~Source() override;

    QString id() const;
    virtual QString kind() const = 0;
    virtual QString name() const = 0;
    bool refreshing() const;
    QString problem() const;
    bool reachable() const;
    // "artists", "albums", "tracks", "playlists" and "favorites" of the
    // cache.
    Q_INVOKABLE QVariantMap counts(bool downloadedOnly) const;

    Q_INVOKABLE bool isFavorite(const QString &trackId) const;
    // Marks the track in the cache at once; a server is told when it can
    // be reached.
    Q_INVOKABLE void setFavorite(const QString &trackId, bool favorite);
    // The player calls this once a track counts as played.
    virtual void reportPlayed(const QString &trackId);

    virtual bool canEditPlaylists() const;
    // Only while the server can be reached; the answer comes as
    // playlistEditFinished.
    Q_INVOKABLE virtual void addToPlaylist(const QString &playlistId, const QStringList &trackIds);
    Q_INVOKABLE virtual void createPlaylist(const QString &name, const QStringList &trackIds);

    int artistCount() const;
    int albumCount() const;
    int trackCount() const;
    int playlistCount() const;

    LibraryDatabase *library() const;
    QString directory() const;

    // Reads the library from the phone's index or the server into the
    // cache; reports through refreshing, problem and libraryChanged.
    Q_INVOKABLE virtual void refresh() = 0;
    // What QtMultimedia plays for a track of the cache: its downloaded
    // file when there is one, else trackUrl.
    QUrl playableUrl(const QVariantMap &track) const;
    virtual QUrl trackUrl(const QVariantMap &track) const = 0;

    // Servers can download tracks into downloadDirectory; the phone's own
    // music needs no download.
    virtual bool canDownload() const;
    virtual QNetworkRequest downloadRequest(const QVariantMap &track) const;
    QString downloadDirectory() const;
    // An image for QML, empty when there is none; size in pixels.
    virtual QUrl imageUrl(const QString &image, int size) const = 0;

signals:
    void nameChanged();
    void refreshingChanged();
    void problemChanged();
    void libraryChanged();
    void reachableChanged();
    void favoritesChanged();
    // problem is empty when the server took the change.
    void playlistEditFinished(const QString &problem);

protected:
    void setRefreshing(bool refreshing);
    void setProblem(const QString &problem);
    void setReachable(bool reachable);
    // Stores a refresh in the cache and tells the pages.
    void store(const LibrarySnapshot &snapshot);
    // Tells the server about a favorite set in the cache.
    virtual void sendFavorite(const QString &trackId, bool favorite);

private:
    QString m_id;
    QString m_directory;
    QScopedPointer<LibraryDatabase> m_library;
    bool m_refreshing = false;
    QString m_problem;
    bool m_reachable = true;
};

#endif // SOURCE_H

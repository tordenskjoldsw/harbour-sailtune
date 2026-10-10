#ifndef JELLYFIN_H
#define JELLYFIN_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

class QNetworkReply;

// The few Jellyfin calls the spike needs: log in, list albums and their
// tracks, and download a track into the app's private storage. The access
// token stays in memory; nothing secret goes to the log.
class Jellyfin : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit Jellyfin(QObject *parent = nullptr);

    bool loggedIn() const;
    bool busy() const;
    QString error() const;

    Q_INVOKABLE void login(const QString &server, const QString &user, const QString &password);
    // Albums sorted by artist; searchTerm filters when not empty.
    Q_INVOKABLE void loadAlbums(const QString &searchTerm);
    Q_INVOKABLE void loadTracks(const QString &albumId);
    // The original file, without transcoding, for QtMultimedia to stream.
    Q_INVOKABLE QUrl streamUrl(const QString &trackId) const;
    // Downloads the original file of a track from tracksLoaded and keeps
    // its details next to it; reports downloadFinished.
    Q_INVOKABLE void download(const QVariantMap &track);
    // Every downloaded track, without the server: its details, or a
    // placeholder for files downloaded before details were kept.
    Q_INVOKABLE QVariantList downloads() const;
    // The local file of a downloaded track, or an empty URL.
    Q_INVOKABLE QUrl localFile(const QString &trackId, const QString &container) const;
    // Appends a line to log.txt in the app's data directory.
    Q_INVOKABLE void log(const QString &line);
    Q_INVOKABLE QString logText() const;
    Q_INVOKABLE void clearLog();

signals:
    void loggedInChanged();
    void busyChanged();
    void errorChanged();
    void albumsLoaded(const QVariantList &albums);
    void tracksLoaded(const QVariantList &tracks);
    void downloadFinished(const QString &trackId, bool ok, qint64 bytes, int milliseconds);

private:
    QNetworkReply *get(const QString &path, const QList<QPair<QString, QString>> &query);
    QByteArray authorization() const;
    void setBusy(bool busy);
    void setError(const QString &error);
    QString musicDirectory() const;

    QNetworkAccessManager m_network;
    QString m_server;
    QString m_token;
    QString m_userId;
    QString m_deviceId;
    QString m_logPath;
    bool m_busy = false;
    QString m_error;
};

#endif // JELLYFIN_H

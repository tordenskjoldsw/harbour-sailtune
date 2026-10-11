#ifndef JELLYFINSOURCE_H
#define JELLYFINSOURCE_H

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QVariantMap>

#include <functional>

#include "jellyfin.h"
#include "source.h"

// A Jellyfin server and the user logged in to it. The address, user and
// device id live in the app's configuration; the access token in an
// owner-only file in the source's folder. The password is never stored.
class JellyfinSource : public Source
{
    Q_OBJECT
    // The server refused the token: the library still shows, a login
    // renews it.
    Q_PROPERTY(bool needsLogin READ needsLogin NOTIFY needsLoginChanged)
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(QString userName READ userName CONSTANT)

public:
    struct Config {
        QString id;
        QString serverName;
        QString address;
        QString userName;
        QString userId;
        QString deviceId;
    };

    JellyfinSource(const Config &config, const QString &directory, QObject *parent = nullptr);

    QString kind() const override;
    QString name() const override;
    bool needsLogin() const;
    QString address() const;
    QString userName() const;
    Config config() const;

    void refresh() override;
    QUrl trackUrl(const QVariantMap &track) const override;
    QUrl imageUrl(const QString &image, int size) const override;

    // Stores a new token after a login and refreshes.
    void renewToken(const QString &token);
    // Ends the session on the server and deletes the token; the caller
    // removes the source.
    void logout();

    static void writeToken(const QString &directory, const QString &token);

signals:
    void needsLoginChanged();

private:
    using ItemsHandler = std::function<void(const QJsonArray &items)>;
    using DoneHandler = std::function<void(Jellyfin::Failure failure)>;

    Jellyfin::Identity identity() const;
    // Every item of a list call, page by page, at most maxItems.
    void fetchAll(const QString &path, const Jellyfin::Query &query, int maxItems,
                  const ItemsHandler &onItems, const DoneHandler &onDone);
    void fetchPage(const QString &path, const Jellyfin::Query &query, int start, int maxItems,
                   const ItemsHandler &onItems, const DoneHandler &onDone);
    void fetchPlaylistTracks(int index);
    void finishRefresh(Jellyfin::Failure failure);
    void setNeedsLogin(bool needsLogin);

    Config m_config;
    QString m_token;
    bool m_needsLogin = false;
    QNetworkAccessManager m_network;
    // The library while a refresh collects it.
    LibrarySnapshot m_collecting;
};

#endif // JELLYFINSOURCE_H

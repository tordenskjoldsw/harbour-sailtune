#ifndef SOURCES_H
#define SOURCES_H

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

#include "jellyfinsource.h"

class Source;

// Every source the app knows and the one that is active: the phone is
// always there, servers are added. The library pages, the player and the
// downloads follow the active source. Servers are kept in servers.json in
// the app's configuration; their tokens in their own folders.
class Sources : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Source *active READ active NOTIFY activeChanged)
    Q_PROPERTY(QVariantList all READ all NOTIFY allChanged)

public:
    explicit Sources(QObject *parent = nullptr);

    Source *active() const;
    // One map per source: "id", "name", "kind", "active", "detail",
    // "needsLogin".
    QVariantList all() const;
    JellyfinSource *jellyfin(const QString &id) const;

    Q_INVOKABLE void activate(const QString &id);
    // Logs out of a server, deletes its cache and token, and forgets it.
    Q_INVOKABLE void remove(const QString &id);

    // Adds a server, or renews the token of the one with config.id, and
    // makes it the active source.
    void storeJellyfin(JellyfinSource::Config config, const QString &token);

signals:
    void activeChanged();
    void allChanged();

private:
    QString configDirectory() const;
    QString sourceDirectory(const QString &id) const;
    void loadServers();
    void saveServers() const;
    void add(Source *source);

    QList<Source *> m_sources;
    Source *m_active = nullptr;
};

#endif // SOURCES_H

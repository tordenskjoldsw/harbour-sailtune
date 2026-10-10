#ifndef SOURCES_H
#define SOURCES_H

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

class Source;

// Every source the app knows and the one that is active: the phone is
// always there, servers are added. The library pages, the player and the
// downloads follow the active source.
class Sources : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Source *active READ active NOTIFY activeChanged)
    Q_PROPERTY(QVariantList all READ all NOTIFY allChanged)

public:
    explicit Sources(QObject *parent = nullptr);

    Source *active() const;
    // One map per source: "id", "name", "kind", "active".
    QVariantList all() const;

    Q_INVOKABLE void activate(const QString &id);

signals:
    void activeChanged();
    void allChanged();

private:
    QString settingsPath() const;
    QString sourceDirectory(const QString &id) const;

    QList<Source *> m_sources;
    Source *m_active = nullptr;
};

#endif // SOURCES_H

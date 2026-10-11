#include "sources.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QVariantMap>

#include "phonesource.h"

namespace {

const QString ActiveKey = QStringLiteral("sources/active");
const QString DownloadedOnlyKey = QStringLiteral("library/downloadedOnly");
const QString ServersFile = QStringLiteral("/servers.json");
const int MaxServers = 32;
const qint64 MaxServersBytes = 1024 * 1024;

QString newId()
{
    return QUuid::createUuid().toString().remove(QLatin1Char('{')).remove(QLatin1Char('}'));
}

} // namespace

Sources::Sources(QObject *parent)
    : QObject(parent)
{
    add(new PhoneSource(sourceDirectory(QString::fromLatin1(PhoneSource::Id)), this));
    loadServers();

    const QString activeId = QSettings(configDirectory() + QStringLiteral("/settings.ini"),
                                       QSettings::IniFormat)
                                 .value(ActiveKey, QString::fromLatin1(PhoneSource::Id))
                                 .toString();
    m_active = m_sources.first();
    for (Source *source : m_sources) {
        if (source->id() == activeId)
            m_active = source;
    }
    m_downloadedOnly = QSettings(configDirectory() + QStringLiteral("/settings.ini"), QSettings::IniFormat)
                           .value(DownloadedOnlyKey, false)
                           .toBool();
    connect(this, &Sources::activeChanged, this, &Sources::showingDownloadedOnlyChanged);
    connect(this, &Sources::downloadedOnlyChanged, this, &Sources::showingDownloadedOnlyChanged);
}

bool Sources::downloadedOnly() const
{
    return m_downloadedOnly;
}

void Sources::setDownloadedOnly(bool downloadedOnly)
{
    if (m_downloadedOnly == downloadedOnly)
        return;
    m_downloadedOnly = downloadedOnly;
    QSettings(configDirectory() + QStringLiteral("/settings.ini"), QSettings::IniFormat)
        .setValue(DownloadedOnlyKey, downloadedOnly);
    emit downloadedOnlyChanged();
}

bool Sources::showingDownloadedOnly() const
{
    return m_active && m_active->canDownload() && (m_downloadedOnly || !m_active->reachable());
}

QString Sources::configDirectory() const
{
    // Inside the configuration folder Sailjail opens for the app; the
    // default QSettings file would lie beside it.
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(directory);
    return directory;
}

QString Sources::sourceDirectory(const QString &id) const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/sources/") + id;
}

void Sources::add(Source *source)
{
    m_sources.append(source);
    connect(source, &Source::nameChanged, this, &Sources::allChanged);
    connect(source, &Source::reachableChanged, this, [this, source] {
        if (source == m_active)
            emit showingDownloadedOnlyChanged();
    });
    if (auto *server = qobject_cast<JellyfinSource *>(source))
        connect(server, &JellyfinSource::needsLoginChanged, this, &Sources::allChanged);
}

void Sources::loadServers()
{
    QFile file(configDirectory() + ServersFile);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxServersBytes)
        return;
    const QJsonArray servers = QJsonDocument::fromJson(file.readAll()).array();
    for (int i = 0; i < servers.size() && i < MaxServers; ++i) {
        const QJsonObject server = servers.at(i).toObject();
        if (server.value(QStringLiteral("kind")).toString() != QLatin1String("jellyfin"))
            continue;
        JellyfinSource::Config config;
        config.id = server.value(QStringLiteral("id")).toString();
        config.serverName = server.value(QStringLiteral("name")).toString();
        config.address = server.value(QStringLiteral("address")).toString();
        config.userName = server.value(QStringLiteral("userName")).toString();
        config.userId = server.value(QStringLiteral("userId")).toString();
        config.deviceId = server.value(QStringLiteral("deviceId")).toString();
        // The id names the source's folder, so it must stay a plain id.
        if (QUuid(QLatin1Char('{') + config.id + QLatin1Char('}')).isNull() || config.address.isEmpty())
            continue;
        add(new JellyfinSource(config, sourceDirectory(config.id), this));
    }
}

void Sources::saveServers() const
{
    QJsonArray servers;
    for (Source *source : m_sources) {
        const auto *server = qobject_cast<JellyfinSource *>(source);
        if (!server)
            continue;
        const JellyfinSource::Config config = server->config();
        QJsonObject entry;
        entry.insert(QStringLiteral("kind"), QStringLiteral("jellyfin"));
        entry.insert(QStringLiteral("id"), config.id);
        entry.insert(QStringLiteral("name"), config.serverName);
        entry.insert(QStringLiteral("address"), config.address);
        entry.insert(QStringLiteral("userName"), config.userName);
        entry.insert(QStringLiteral("userId"), config.userId);
        entry.insert(QStringLiteral("deviceId"), config.deviceId);
        servers.append(entry);
    }
    QSaveFile file(configDirectory() + ServersFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(servers).toJson());
        file.commit();
    }
}

Source *Sources::active() const
{
    return m_active;
}

QVariantList Sources::all() const
{
    QVariantList list;
    for (Source *source : m_sources) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), source->id());
        entry.insert(QStringLiteral("name"), source->name());
        entry.insert(QStringLiteral("kind"), source->kind());
        entry.insert(QStringLiteral("active"), source == m_active);
        const auto *server = qobject_cast<JellyfinSource *>(source);
        entry.insert(QStringLiteral("detail"), server ? server->userName() + QStringLiteral(" @ ")
                                                            + QUrl(server->address()).host()
                                                      : QString());
        entry.insert(QStringLiteral("needsLogin"), server && server->needsLogin());
        list.append(entry);
    }
    return list;
}

Source *Sources::source(const QString &id) const
{
    for (Source *source : m_sources) {
        if (source->id() == id)
            return source;
    }
    return nullptr;
}

QList<Source *> Sources::list() const
{
    return m_sources;
}

JellyfinSource *Sources::jellyfin(const QString &id) const
{
    for (Source *source : m_sources) {
        if (source->id() == id)
            return qobject_cast<JellyfinSource *>(source);
    }
    return nullptr;
}

void Sources::activate(const QString &id)
{
    for (Source *source : m_sources) {
        if (source->id() != id || source == m_active)
            continue;
        m_active = source;
        QSettings(configDirectory() + QStringLiteral("/settings.ini"), QSettings::IniFormat)
            .setValue(ActiveKey, id);
        emit activeChanged();
        emit allChanged();
        return;
    }
}

void Sources::remove(const QString &id)
{
    JellyfinSource *server = jellyfin(id);
    if (!server)
        return;
    if (m_active == server)
        activate(QString::fromLatin1(PhoneSource::Id));
    server->logout();
    m_sources.removeOne(server);
    const QString directory = server->directory();
    server->deleteLater();
    QDir(directory).removeRecursively();
    saveServers();
    emit allChanged();
}

void Sources::storeJellyfin(JellyfinSource::Config config, const QString &token)
{
    if (JellyfinSource *existing = jellyfin(config.id)) {
        existing->renewToken(token);
        activate(existing->id());
        emit allChanged();
        return;
    }
    config.id = newId();
    const QString directory = sourceDirectory(config.id);
    auto *server = new JellyfinSource(config, directory, this);
    JellyfinSource::writeToken(directory, token);
    server->renewToken(token);
    add(server);
    saveServers();
    activate(server->id());
    emit allChanged();
}

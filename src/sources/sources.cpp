#include "sources.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

#include "phonesource.h"

namespace {

const QString ActiveKey = QStringLiteral("sources/active");

} // namespace

Sources::Sources(QObject *parent)
    : QObject(parent)
{
    auto *phone = new PhoneSource(sourceDirectory(QString::fromLatin1(PhoneSource::Id)), this);
    m_sources.append(phone);

    const QString activeId = QSettings(settingsPath(), QSettings::IniFormat)
                                 .value(ActiveKey, QString::fromLatin1(PhoneSource::Id))
                                 .toString();
    m_active = phone;
    for (Source *source : m_sources) {
        if (source->id() == activeId)
            m_active = source;
    }
}

QString Sources::settingsPath() const
{
    // Inside the configuration folder Sailjail opens for the app; the
    // default QSettings file would lie beside it.
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(directory);
    return directory + QStringLiteral("/settings.ini");
}

QString Sources::sourceDirectory(const QString &id) const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/sources/") + id;
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
        list.append(entry);
    }
    return list;
}

void Sources::activate(const QString &id)
{
    for (Source *source : m_sources) {
        if (source->id() != id || source == m_active)
            continue;
        m_active = source;
        QSettings(settingsPath(), QSettings::IniFormat).setValue(ActiveKey, id);
        emit activeChanged();
        emit allChanged();
        return;
    }
}

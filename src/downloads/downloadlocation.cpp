#include "downloadlocation.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>

namespace {

const QString OnCardKey = QStringLiteral("downloadsOnCard");
// Hidden, so that the media index skips it; .nomedia inside keeps out
// the indexers that look for that file instead.
const QString CardFolder = QStringLiteral("/.sailtune");

QString settingsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/settings.ini");
}

// Where Sailfish mounts memory cards for the user.
QString mediaDirectory()
{
    return QStringLiteral("/run/media/") + QDir::home().dirName();
}

} // namespace

DownloadLocation::DownloadLocation(QObject *parent)
    : QObject(parent)
{
    m_onCard = QSettings(settingsPath(), QSettings::IniFormat).value(OnCardKey, false).toBool();
    // A card's folder appears here when it is mounted and goes when it is
    // removed.
    m_watcher.addPath(mediaDirectory());
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &DownloadLocation::checkCard);
    // Without a card since boot the folder may be missing, so there is
    // nothing to watch; the app looks again whenever it comes to the front.
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            checkCard();
    });
    checkCard();
}

bool DownloadLocation::onCard() const
{
    return m_onCard;
}

void DownloadLocation::setOnCard(bool onCard)
{
    if (m_onCard == onCard)
        return;
    m_onCard = onCard;
    QSettings(settingsPath(), QSettings::IniFormat).setValue(OnCardKey, onCard);
    emit changed();
}

QString DownloadLocation::cardRoot() const
{
    return m_cardRoot;
}

QString DownloadLocation::directoryFor(const QString &sourceId, const QString &phoneDirectory) const
{
    if (!m_onCard)
        return phoneDirectory;
    const QString directory = cardDirectoryFor(sourceId);
    if (directory.isEmpty())
        return QString();
    const QString folder = m_cardRoot + CardFolder;
    if (QDir().mkpath(directory) && !QFile::exists(folder + QStringLiteral("/.nomedia"))) {
        QFile marker(folder + QStringLiteral("/.nomedia"));
        if (marker.open(QIODevice::WriteOnly))
            marker.close();
    }
    return directory;
}

QString DownloadLocation::cardDirectoryFor(const QString &sourceId) const
{
    if (m_cardRoot.isEmpty() || sourceId.isEmpty())
        return QString();
    return m_cardRoot + CardFolder + QStringLiteral("/downloads/") + sourceId;
}

QStringList DownloadLocation::presentRoots() const
{
    QStringList roots;
    roots.append(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1Char('/'));
    if (!m_cardRoot.isEmpty())
        roots.append(m_cardRoot + QLatin1Char('/'));
    return roots;
}

void DownloadLocation::checkCard()
{
    // The watch ends when the folder was gone for a moment.
    if (m_watcher.directories().isEmpty())
        m_watcher.addPath(mediaDirectory());
    QString root;
    const QString prefix = mediaDirectory() + QLatin1Char('/');
    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        if (volume.isValid() && volume.isReady() && !volume.isReadOnly()
            && volume.rootPath().startsWith(prefix)) {
            root = volume.rootPath();
            break;
        }
    }
    if (root == m_cardRoot)
        return;
    m_cardRoot = root;
    emit changed();
}

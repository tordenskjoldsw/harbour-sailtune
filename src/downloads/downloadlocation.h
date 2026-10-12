#ifndef DOWNLOADLOCATION_H
#define DOWNLOADLOCATION_H

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>

// Where new downloads go: the app's private storage on the phone, or the
// memory card when the user chose it and one is in the phone. Downloads
// already saved stay where they are; each one's file path is in its
// source's library. On the card they live in a hidden folder that the
// media index skips, so they do not show up again as the phone's music.
class DownloadLocation : public QObject
{
    Q_OBJECT

public:
    explicit DownloadLocation(QObject *parent = nullptr);

    // The user's choice; kept in settings.ini.
    bool onCard() const;
    void setOnCard(bool onCard);
    // The root of the memory card in the phone, empty without one.
    QString cardRoot() const;
    // The folder new downloads of a source go to: phoneDirectory, or the
    // source's folder on the card. Empty when the card was chosen but is
    // not in the phone.
    QString directoryFor(const QString &sourceId, const QString &phoneDirectory) const;
    // The source's folder on the card in the phone, empty without one.
    QString cardDirectoryFor(const QString &sourceId) const;
    // Path prefixes under which saved downloads can be read now.
    QStringList presentRoots() const;

signals:
    // The choice changed, or a card came or went.
    void changed();

private:
    void checkCard();

    bool m_onCard = false;
    QString m_cardRoot;
    QFileSystemWatcher m_watcher;
};

#endif // DOWNLOADLOCATION_H

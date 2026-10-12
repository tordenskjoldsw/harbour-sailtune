#ifndef DOWNLOADS_H
#define DOWNLOADS_H

#include <QFile>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkConfigurationManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "downloadlocation.h"

class QNetworkReply;
class Source;
class Sources;

// Downloads tracks of a source for playing without a network, one after
// the other while the app runs. The queue survives closing the app and
// losing the connection: without the server the downloads pause, keep
// the part of the file they have, retry every minute and continue from
// that byte. A file is checked against its size and only then renamed;
// downloads pause before the free space where they go falls below the
// reserve. New downloads go to the phone or, by choice, the memory card.
class Downloads : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY runningChanged)
    Q_PROPERTY(int waiting READ waiting NOTIFY progressChanged)
    Q_PROPERTY(QString current READ current NOTIFY progressChanged)
    Q_PROPERTY(int percent READ percent NOTIFY progressChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)
    // Changes whenever a download is added or removed, so lists reload.
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    // The user's choice to save new downloads on the memory card.
    Q_PROPERTY(bool onCard READ onCard WRITE setOnCard NOTIFY locationChanged)
    // Whether a memory card is in the phone.
    Q_PROPERTY(bool cardPresent READ cardPresent NOTIFY locationChanged)

public:
    explicit Downloads(Sources *sources, QObject *parent = nullptr);
    ~Downloads() override;

    bool running() const;
    bool paused() const;
    int waiting() const;
    QString current() const;
    int percent() const;
    QString problem() const;
    int revision() const;
    bool onCard() const;
    // A download running on the other side starts again from the
    // beginning at the new place.
    void setOnCard(bool onCard);
    bool cardPresent() const;

    // Queues the tracks (LibraryModel rows) of the active source that are
    // not downloaded yet.
    Q_INVOKABLE void download(const QVariantList &tracks);
    // Queues every track of the active source that is not downloaded yet.
    Q_INVOKABLE void downloadAll();
    // Deletes the downloaded files of these tracks of the active source
    // and drops them from the queue.
    Q_INVOKABLE void remove(const QVariantList &tracks);
    // Tries a paused queue again at once.
    Q_INVOKABLE void resume();

    // For the settings: "used" (bytes of every source's downloads),
    // "free" (bytes free on the phone) and "cardFree" (bytes free on the
    // memory card, -1 without one).
    Q_INVOKABLE QVariantMap storage() const;
    // Bytes of one source's downloads, as a number QML can hold.
    Q_INVOKABLE double sourceBytes(const QString &sourceId) const;
    // Every download of every source, and the queue.
    Q_INVOKABLE void removeAll();

signals:
    void runningChanged();
    void progressChanged();
    void problemChanged();
    void revisionChanged();
    void locationChanged();

private:
    enum class Outcome { Done, Pause, Skip, Restart };

    void startNext();
    // Ends the running reply; its finished handler then does this.
    void abortReply(Outcome outcome, const QString &problem);
    void finish(Outcome outcome, const QString &problem);
    void pause(const QString &problem);
    void setProblem(const QString &problem);
    void bumpRevision();
    void saveQueue() const;
    void loadQueue();
    QString queuePath() const;
    QString partPath(const QVariantMap &track) const;
    // Deletes the part files of a track on the phone and on the card.
    void removeParts(Source *source, const QString &trackId) const;
    // Tells every library which saved downloads can be read now.
    void applyPresentRoots();
    void onLocationChanged();
    QString spaceProblem() const;

    Sources *m_sources;
    QPointer<Source> m_source;
    DownloadLocation m_location;
    // Where the running download goes.
    QString m_directory;
    QList<QVariantMap> m_queue;
    QVariantMap m_track;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QFile m_file;
    QString m_finalPath;
    // Bytes the part file had when this request started.
    qint64 m_offset = 0;
    qint64 m_expected = -1;
    qint64 m_received = 0;
    // Whether this side aborted the reply, why, and what follows.
    bool m_aborted = false;
    QString m_abortProblem;
    Outcome m_abortOutcome = Outcome::Pause;
    bool m_paused = false;
    QTimer m_retry;
    // Tells when the phone is online again, e.g. back in the home WLAN.
    QNetworkConfigurationManager m_networkState;
    QString m_problem;
    int m_revision = 0;
};

#endif // DOWNLOADS_H

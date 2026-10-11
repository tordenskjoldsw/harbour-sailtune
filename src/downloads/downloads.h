#ifndef DOWNLOADS_H
#define DOWNLOADS_H

#include <QElapsedTimer>
#include <QFile>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

class QNetworkReply;
class Source;
class Sources;

// Downloads tracks of the active source for playing without a network,
// one after the other while the app runs. A file is written beside its
// final name, checked against the size the server announced and only
// then renamed; downloads stop before the phone's free space falls below
// the reserve.
class Downloads : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int waiting READ waiting NOTIFY progressChanged)
    Q_PROPERTY(QString current READ current NOTIFY progressChanged)
    Q_PROPERTY(int percent READ percent NOTIFY progressChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)
    // Changes whenever a download is added or removed, so lists reload.
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit Downloads(Sources *sources, QObject *parent = nullptr);
    ~Downloads() override;

    bool running() const;
    int waiting() const;
    QString current() const;
    int percent() const;
    QString problem() const;
    int revision() const;

    // Queues the tracks (LibraryModel rows) of the active source that are
    // not downloaded yet.
    Q_INVOKABLE void download(const QVariantList &tracks);
    // Deletes the downloaded files of these tracks.
    Q_INVOKABLE void remove(const QVariantList &tracks);
    Q_INVOKABLE void cancel();

signals:
    void runningChanged();
    void progressChanged();
    void problemChanged();
    void revisionChanged();

private:
    void startNext();
    void finishCurrent(bool ok, const QString &problem);
    void setProblem(const QString &problem);
    void bumpRevision();

    Sources *m_sources;
    QPointer<Source> m_source;
    QList<QVariantMap> m_queue;
    QVariantMap m_track;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QFile m_file;
    QString m_finalPath;
    qint64 m_expected = -1;
    qint64 m_received = 0;
    QString m_problem;
    int m_revision = 0;
};

#endif // DOWNLOADS_H

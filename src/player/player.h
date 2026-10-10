#ifndef PLAYER_H
#define PLAYER_H

#include <QMediaPlayer>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class Source;
class Sources;

// The queue and the one media player of the app. QML shows what it
// exposes and calls its slots; the lock screen and the cover use the same
// calls. The queue, the track and the position survive closing the app.
class Player : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY trackChanged)
    Q_PROPERTY(QString trackId READ trackId NOTIFY trackChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QUrl image READ image NOTIFY trackChanged)
    Q_PROPERTY(int queueIndex READ queueIndex NOTIFY trackChanged)
    Q_PROPERTY(int queueLength READ queueLength NOTIFY queueChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)

public:
    Player(Sources *sources, QObject *parent = nullptr);
    ~Player() override;

    bool hasTrack() const;
    QString trackId() const;
    QString title() const;
    QString artist() const;
    QString album() const;
    QUrl image() const;
    int queueIndex() const;
    int queueLength() const;
    bool playing() const;
    qint64 position() const;
    qint64 duration() const;
    bool seekable() const;
    QString problem() const;

    // Plays tracks of the active source's cache (LibraryModel rows) from
    // index on, replacing the queue.
    Q_INVOKABLE void playTracks(const QVariantList &tracks, int index);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void next();
    // To the start of the track, or to the track before when it has just
    // begun.
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 milliseconds);
    Q_INVOKABLE void seekBy(qint64 milliseconds);

signals:
    void trackChanged();
    void queueChanged();
    void playingChanged();
    void positionChanged();
    void durationChanged();
    void seekableChanged();
    void problemChanged();

private:
    void load(int index, bool start);
    void onStateChanged(QMediaPlayer::State state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void setProblem(const QString &problem);
    void saveQueue();
    // Only the track and position, the small write made while playing.
    void savePosition();
    // The position to keep: the one still to be resumed, else the player's.
    qint64 currentPosition() const;
    // Seeks to the restored position once the player can seek.
    void resumeIfReady();
    void restoreQueue();
    QString settingsPath() const;

    Sources *m_sources;
    QPointer<Source> m_source;
    QMediaPlayer m_media;
    QVariantList m_queue;
    int m_index = -1;
    // A position to seek to once the restored track has loaded.
    qint64 m_resumeAt = -1;
    // When the position was last written while playing.
    qint64 m_savedAt = 0;
    // The last position while playing or paused. A stopped player reports
    // 0, and the system stops it when the app closes (seen on the device),
    // so the saved position must not follow it.
    qint64 m_lastPosition = 0;
    QString m_problem;
};

#endif // PLAYER_H

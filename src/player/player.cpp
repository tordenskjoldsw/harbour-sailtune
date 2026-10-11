#include "player.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

#include "../library/librarydatabase.h"
#include "../sources/source.h"
#include "../sources/sources.h"

namespace {

// "Previous" restarts the track after this much of it has played.
const qint64 RestartThresholdMs = 3000;
const int PositionIntervalMs = 500;
const int MaxSavedTracks = 2000;
// Sailfish may end the app without aboutToQuit when it is swiped away,
// so the position is also written while playing: two numbers, rarely.
const qint64 SaveIntervalMs = 15000;
// The rule of the common scrobbling services.
const qint64 PlayedAfterMs = 4 * 60 * 1000;
// A larger step of the position is a seek, not listening.
const qint64 MaxListenStepMs = 2000;

const QString SourceKey = QStringLiteral("queue/source");
const QString TracksKey = QStringLiteral("queue/tracks");
const QString IndexKey = QStringLiteral("queue/index");
const QString PositionKey = QStringLiteral("queue/position");

QString text(const QVariantMap &track, const char *key)
{
    return track.value(QLatin1String(key)).toString();
}

} // namespace

Player::Player(Sources *sources, QObject *parent)
    : QObject(parent)
    , m_sources(sources)
{
    m_media.setAudioRole(QAudio::MusicRole);
    m_media.setNotifyInterval(PositionIntervalMs);
    connect(&m_media, &QMediaPlayer::stateChanged, this, &Player::onStateChanged);
    connect(&m_media, &QMediaPlayer::mediaStatusChanged, this, &Player::onMediaStatusChanged);
    connect(&m_media, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (m_media.state() == QMediaPlayer::PlayingState)
            countListening(position - m_lastPosition);
        if (m_media.state() != QMediaPlayer::StoppedState)
            m_lastPosition = position;
        emit positionChanged();
        resumeIfReady();
        if (m_resumeAt < 0 && qAbs(position - m_savedAt) >= SaveIntervalMs) {
            m_savedAt = position;
            savePosition();
        }
    });
    connect(&m_media, &QMediaPlayer::seekableChanged, this, &Player::resumeIfReady);
    connect(&m_media, &QMediaPlayer::durationChanged, this, &Player::durationChanged);
    connect(&m_media, &QMediaPlayer::seekableChanged, this, &Player::seekableChanged);
    connect(this, &Player::trackChanged, this, &Player::favoriteChanged);
    // QMediaPlayer::error is overloaded with the getter; the signal is
    // picked by its signature.
    connect(&m_media, static_cast<void (QMediaPlayer::*)(QMediaPlayer::Error)>(&QMediaPlayer::error),
            this, [this](QMediaPlayer::Error) { setProblem(m_media.errorString()); });
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Player::saveQueue);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                if (state != Qt::ApplicationActive)
                    savePosition();
            });
    restoreQueue();
}

Player::~Player()
{
    // Stopping on the way out would report position 0.
    disconnect(&m_media, nullptr, this, nullptr);
    savePosition();
}

bool Player::hasTrack() const
{
    return m_index >= 0 && m_index < m_queue.size();
}

QString Player::trackId() const
{
    return hasTrack() ? text(m_queue.at(m_index).toMap(), "id") : QString();
}

QString Player::title() const
{
    return hasTrack() ? text(m_queue.at(m_index).toMap(), "title") : QString();
}

QString Player::artist() const
{
    return hasTrack() ? text(m_queue.at(m_index).toMap(), "artist") : QString();
}

QString Player::album() const
{
    return hasTrack() ? text(m_queue.at(m_index).toMap(), "album") : QString();
}

QUrl Player::image() const
{
    if (!hasTrack() || !m_source)
        return QUrl();
    return m_source->imageUrl(text(m_queue.at(m_index).toMap(), "image"), 512);
}

int Player::queueIndex() const
{
    return m_index;
}

int Player::queueLength() const
{
    return m_queue.size();
}

bool Player::playing() const
{
    return m_media.state() == QMediaPlayer::PlayingState;
}

qint64 Player::position() const
{
    return m_media.state() == QMediaPlayer::StoppedState ? currentPosition() : m_media.position();
}

qint64 Player::duration() const
{
    return m_media.duration();
}

bool Player::seekable() const
{
    return m_media.isSeekable();
}

QString Player::problem() const
{
    return m_problem;
}

bool Player::favorite() const
{
    return hasTrack() && m_source && m_source->isFavorite(trackId());
}

void Player::toggleFavorite()
{
    if (hasTrack() && m_source)
        m_source->setFavorite(trackId(), !favorite());
}

void Player::setSource(Source *source)
{
    if (m_source == source)
        return;
    if (m_source)
        disconnect(m_source, nullptr, this, nullptr);
    m_source = source;
    if (m_source)
        connect(m_source, &Source::favoritesChanged, this, &Player::favoriteChanged);
}

void Player::countListening(qint64 step)
{
    if (m_reported || step <= 0 || step > MaxListenStepMs || !hasTrack() || !m_source)
        return;
    m_listened += step;
    const qint64 duration = m_media.duration();
    const qint64 needed = duration > 0 ? qMin(duration / 2, PlayedAfterMs) : PlayedAfterMs;
    if (m_listened >= needed) {
        m_reported = true;
        m_source->reportPlayed(trackId());
    }
}

void Player::playTracks(const QVariantList &tracks, int index)
{
    if (tracks.isEmpty() || index < 0 || index >= tracks.size())
        return;
    setSource(m_sources->active());
    m_queue = tracks;
    m_resumeAt = -1;
    emit queueChanged();
    load(index, true);
    saveQueue();
}

void Player::load(int index, bool start)
{
    if (index < 0 || index >= m_queue.size() || !m_source) {
        m_media.stop();
        return;
    }
    m_index = index;
    m_savedAt = 0;
    m_lastPosition = 0;
    m_listened = 0;
    m_reported = false;
    setProblem(QString());
    m_media.setMedia(m_source->playableUrl(m_queue.at(index).toMap()));
    emit trackChanged();
    if (start)
        m_media.play();
}

void Player::play()
{
    if (hasTrack())
        m_media.play();
}

void Player::pause()
{
    m_media.pause();
}

void Player::togglePlay()
{
    if (playing())
        pause();
    else
        play();
}

void Player::next()
{
    if (m_index + 1 < m_queue.size()) {
        m_resumeAt = -1;
        load(m_index + 1, true);
        saveQueue();
    } else {
        m_media.stop();
    }
}

void Player::previous()
{
    m_resumeAt = -1;
    if (m_media.position() > RestartThresholdMs || m_index == 0) {
        m_media.setPosition(0);
        return;
    }
    load(m_index - 1, playing() || m_media.state() == QMediaPlayer::StoppedState);
    saveQueue();
}

void Player::seek(qint64 milliseconds)
{
    if (!m_media.isSeekable())
        return;
    m_resumeAt = -1;
    const qint64 end = qMax<qint64>(0, m_media.duration() - 1000);
    m_media.setPosition(qBound<qint64>(0, milliseconds, end));
}

void Player::seekBy(qint64 milliseconds)
{
    seek(currentPosition() + milliseconds);
}

void Player::onStateChanged(QMediaPlayer::State state)
{
    emit playingChanged();
    // The player reports the end of a track before it stops; a track
    // started on that report would be stopped again (spike, 2026-10-10).
    if (state == QMediaPlayer::StoppedState && m_media.mediaStatus() == QMediaPlayer::EndOfMedia)
        next();
    if (state == QMediaPlayer::PausedState)
        savePosition();
}

void Player::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::InvalidMedia)
        setProblem(tr("This track cannot be played"));
    if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
        resumeIfReady();
}

void Player::resumeIfReady()
{
    if (m_resumeAt < 0 || !m_media.isSeekable())
        return;
    const qint64 target = m_resumeAt;
    m_resumeAt = -1;
    m_savedAt = target;
    m_lastPosition = target;
    m_media.setPosition(target);
}

qint64 Player::currentPosition() const
{
    return m_resumeAt >= 0 ? m_resumeAt : m_lastPosition;
}

void Player::setProblem(const QString &problem)
{
    if (m_problem == problem)
        return;
    m_problem = problem;
    emit problemChanged();
}

QString Player::settingsPath() const
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(directory);
    return directory + QStringLiteral("/settings.ini");
}

void Player::saveQueue()
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    if (!hasTrack() || !m_source) {
        settings.remove(QStringLiteral("queue"));
        return;
    }
    QStringList ids;
    for (int i = 0; i < m_queue.size() && i < MaxSavedTracks; ++i)
        ids.append(text(m_queue.at(i).toMap(), "id"));
    settings.setValue(SourceKey, m_source->id());
    settings.setValue(TracksKey, ids);
    settings.setValue(IndexKey, m_index);
    settings.setValue(PositionKey, currentPosition());
}

void Player::savePosition()
{
    if (!hasTrack() || !m_source)
        return;
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(IndexKey, m_index);
    settings.setValue(PositionKey, currentPosition());
}

void Player::restoreQueue()
{
    const QSettings settings(settingsPath(), QSettings::IniFormat);
    Source *source = m_sources->active();
    if (!source || settings.value(SourceKey).toString() != source->id())
        return;
    const QVariantList tracks = source->library()->tracksByIds(settings.value(TracksKey).toStringList());
    const int index = settings.value(IndexKey).toInt();
    if (tracks.isEmpty() || index < 0 || index >= tracks.size())
        return;
    setSource(source);
    m_queue = tracks;
    emit queueChanged();
    // Set before loading: loading saves the position, which must stay the
    // restored one until the player can seek to it.
    m_resumeAt = settings.value(PositionKey).toLongLong();
    load(index, false);
}

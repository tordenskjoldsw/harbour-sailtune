#include "jellyfinsource.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSaveFile>
#include <QUrlQuery>

namespace {

const int PageSize = 500;
// Far beyond a personal collection; a server answering more is not
// trusted further.
const int MaxArtists = 50000;
const int MaxAlbums = 100000;
const int MaxTracks = 300000;
const int MaxPlaylists = 1000;
const int MaxPlaylistTracks = 10000;
const qint64 TicksPerSecond = 10000000;
const int MaxTextLength = 1024;
const QString UnknownAlbumId = QStringLiteral("unknown-album");
const QString TokenFile = QStringLiteral("/token");
const QString PendingFile = QStringLiteral("/pending.json");
// Years of plays without a connection; beyond that the oldest go.
const int MaxPending = 5000;
const qint64 MaxPendingBytes = 2 * 1024 * 1024;
const int MaxPlaylistName = 200;
const QString PlayedAction = QStringLiteral("played");
const QString FavoriteAction = QStringLiteral("favorite");
// Covers kept for offline use, at the size the player page shows.
const int KeptCoverSize = 512;
const qint64 MaxCoverBytes = 5 * 1024 * 1024;

// Text from the server, cut to a sane length.
QString text(const QJsonObject &item, const char *key)
{
    return item.value(QLatin1String(key)).toString().left(MaxTextLength);
}

// "<item id>|<tag>" for the item's primary image, or empty.
QString primaryImage(const QJsonObject &item, const QString &itemId)
{
    const QString tag = item.value(QStringLiteral("ImageTags")).toObject()
                            .value(QStringLiteral("Primary")).toString();
    return tag.isEmpty() ? QString() : itemId + QLatin1Char('|') + tag;
}

const Jellyfin::Query CommonQuery = {
    {QStringLiteral("Recursive"), QStringLiteral("true")},
    {QStringLiteral("EnableImageTypes"), QStringLiteral("Primary")},
    {QStringLiteral("ImageTypeLimit"), QStringLiteral("1")},
};

const QPair<QString, QString> WithoutUserData = {QStringLiteral("EnableUserData"), QStringLiteral("false")};

} // namespace

JellyfinSource::JellyfinSource(const Config &config, const QString &directory, QObject *parent)
    : Source(config.id, directory, parent)
    , m_config(config)
{
    QFile file(directory + TokenFile);
    if (file.open(QIODevice::ReadOnly))
        m_token = QString::fromUtf8(file.readAll()).trimmed().left(MaxTextLength);
    m_needsLogin = m_token.isEmpty();
    loadPending();
}

QString JellyfinSource::kind() const
{
    return QStringLiteral("jellyfin");
}

QString JellyfinSource::name() const
{
    return m_config.serverName;
}

bool JellyfinSource::needsLogin() const
{
    return m_needsLogin;
}

QString JellyfinSource::address() const
{
    return m_config.address;
}

QString JellyfinSource::userName() const
{
    return m_config.userName;
}

JellyfinSource::Config JellyfinSource::config() const
{
    return m_config;
}

Jellyfin::Identity JellyfinSource::identity() const
{
    return {m_config.deviceId, m_token};
}

void JellyfinSource::writeToken(const QString &directory, const QString &token)
{
    QSaveFile file(directory + TokenFile);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    file.write(token.toUtf8());
    file.commit();
}

void JellyfinSource::renewToken(const QString &token)
{
    m_token = token;
    writeToken(directory(), token);
    setNeedsLogin(false);
    setProblem(QString());
    refresh();
}

void JellyfinSource::logout()
{
    if (!m_token.isEmpty()) {
        // The answer does not matter; the token is gone on this side.
        Jellyfin::request(&m_network, QStringLiteral("POST"),
                          Jellyfin::url(m_config.address, QStringLiteral("/Sessions/Logout")),
                          identity(), QByteArray(), [](Jellyfin::Failure, const QJsonObject &) {});
    }
    m_token.clear();
    QFile::remove(directory() + TokenFile);
}

void JellyfinSource::setNeedsLogin(bool needsLogin)
{
    if (m_needsLogin == needsLogin)
        return;
    m_needsLogin = needsLogin;
    emit needsLoginChanged();
}

QUrl JellyfinSource::trackUrl(const QVariantMap &track) const
{
    // QtMultimedia sends no custom headers, so the token travels in the
    // URL; nothing that holds this URL is ever logged.
    return Jellyfin::url(m_config.address,
                         QStringLiteral("/Audio/%1/stream").arg(track.value(QStringLiteral("id")).toString()),
                         {{QStringLiteral("static"), QStringLiteral("true")},
                          {QStringLiteral("api_key"), m_token}});
}

QUrl JellyfinSource::imageUrl(const QString &image, int size) const
{
    const QString cover = coverFile(image);
    if (!cover.isEmpty() && QFileInfo::exists(cover))
        return QUrl::fromLocalFile(cover);
    return serverImageUrl(image, size);
}

void JellyfinSource::keepCovers()
{
    const QStringList images = library()->downloadedImages();
    pruneCovers(images);
    m_missingCovers.clear();
    for (const QString &image : images) {
        const QString cover = coverFile(image);
        if (!cover.isEmpty() && !QFileInfo::exists(cover))
            m_missingCovers.append(image);
    }
    if (!m_fetchingCovers)
        fetchNextCover();
}

void JellyfinSource::fetchNextCover()
{
    if (m_missingCovers.isEmpty()) {
        m_fetchingCovers = false;
        return;
    }
    m_fetchingCovers = true;
    const QString image = m_missingCovers.takeFirst();
    QNetworkReply *reply = m_network.get(QNetworkRequest(serverImageUrl(image, KeptCoverSize)));
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > MaxCoverBytes || total > MaxCoverBytes)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, image] {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
            && reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith(QLatin1String("image/"));
        if (!ok && reply->error() != QNetworkReply::NoError
            && reply->error() != QNetworkReply::OperationCanceledError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 0) {
            // No connection: the next download or refresh tries again.
            m_missingCovers.clear();
            m_fetchingCovers = false;
            return;
        }
        const QByteArray data = ok ? reply->readAll() : QByteArray();
        const QString cover = coverFile(image);
        if (!data.isEmpty() && data.size() <= MaxCoverBytes && !cover.isEmpty()) {
            QDir().mkpath(QFileInfo(cover).path());
            QSaveFile file(cover);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.commit();
            }
        }
        fetchNextCover();
    });
}

QUrl JellyfinSource::serverImageUrl(const QString &image, int size) const
{
    const int separator = image.indexOf(QLatin1Char('|'));
    if (separator <= 0)
        return QUrl();
    return Jellyfin::url(m_config.address,
                         QStringLiteral("/Items/%1/Images/Primary").arg(image.left(separator)),
                         {{QStringLiteral("tag"), image.mid(separator + 1)},
                          {QStringLiteral("maxWidth"), QString::number(size)},
                          {QStringLiteral("quality"), QStringLiteral("90")}});
}

bool JellyfinSource::canDownload() const
{
    return true;
}

QNetworkRequest JellyfinSource::downloadRequest(const QVariantMap &track) const
{
    // The original file, as streaming plays it; the token goes in the
    // header here, not in the URL.
    QNetworkRequest request(Jellyfin::url(
        m_config.address, QStringLiteral("/Audio/%1/stream").arg(track.value(QStringLiteral("id")).toString()),
        {{QStringLiteral("static"), QStringLiteral("true")}}));
    request.setRawHeader("Authorization", Jellyfin::authorization(identity()));
    return request;
}

void JellyfinSource::fetchAll(const QString &path, const Jellyfin::Query &query, int maxItems,
                              const ItemsHandler &onItems, const DoneHandler &onDone)
{
    fetchPage(path, query, 0, maxItems, onItems, onDone);
}

void JellyfinSource::fetchPage(const QString &path, const Jellyfin::Query &query, int start,
                               int maxItems, const ItemsHandler &onItems, const DoneHandler &onDone)
{
    Jellyfin::Query pageQuery = query;
    pageQuery.append({QStringLiteral("StartIndex"), QString::number(start)});
    pageQuery.append({QStringLiteral("Limit"), QString::number(PageSize)});
    Jellyfin::request(&m_network, QStringLiteral("GET"), Jellyfin::url(m_config.address, path, pageQuery),
                      identity(), QByteArray(),
                      [=](Jellyfin::Failure failure, const QJsonObject &answer) {
                          if (failure != Jellyfin::Failure::None) {
                              onDone(failure);
                              return;
                          }
                          const QJsonArray items = answer.value(QStringLiteral("Items")).toArray();
                          const int total = answer.value(QStringLiteral("TotalRecordCount")).toInt();
                          onItems(items);
                          const int next = start + items.size();
                          if (items.isEmpty() || next >= total)
                              onDone(Jellyfin::Failure::None);
                          else if (next >= maxItems)
                              onDone(Jellyfin::Failure::BadAnswer);
                          else
                              fetchPage(path, query, next, maxItems, onItems, onDone);
                      });
}

void JellyfinSource::refresh()
{
    if (refreshing() || m_token.isEmpty()) {
        if (m_token.isEmpty())
            setNeedsLogin(true);
        return;
    }
    setRefreshing(true);
    m_collecting = LibrarySnapshot();
    m_serverFavorites.clear();
    m_favoritesDuringRefresh.clear();
    // The library read afterwards then already has the favorites and
    // plays made without a connection.
    flushPending([this] { fetchLibrary(); });
}

void JellyfinSource::fetchLibrary()
{
    Jellyfin::Query query = CommonQuery;
    query.append({QStringLiteral("userId"), m_config.userId});

    // Artists, then albums, then tracks, then playlists; each step starts
    // when the one before has every page.
    auto playlists = [this, query] {
        Jellyfin::Query playlistQuery = query;
        playlistQuery.append(WithoutUserData);
        playlistQuery.append({QStringLiteral("IncludeItemTypes"), QStringLiteral("Playlist")});
        fetchAll(QStringLiteral("/Items"), playlistQuery, MaxPlaylists,
                 [this](const QJsonArray &items) {
                     for (const QJsonValue &value : items) {
                         const QJsonObject item = value.toObject();
                         LibraryPlaylist playlist;
                         playlist.id = text(item, "Id");
                         playlist.name = text(item, "Name");
                         playlist.image = primaryImage(item, playlist.id);
                         if (!playlist.id.isEmpty())
                             m_collecting.playlists.append(playlist);
                     }
                 },
                 [this](Jellyfin::Failure failure) {
                     if (failure != Jellyfin::Failure::None)
                         finishRefresh(failure);
                     else
                         fetchPlaylistTracks(0);
                 });
    };
    auto tracks = [this, query, playlists] {
        // The user data carries the favorite mark.
        Jellyfin::Query trackQuery = query;
        trackQuery.append({QStringLiteral("EnableUserData"), QStringLiteral("true")});
        trackQuery.append({QStringLiteral("IncludeItemTypes"), QStringLiteral("Audio")});
        fetchAll(QStringLiteral("/Items"), trackQuery, MaxTracks,
                 [this](const QJsonArray &items) {
                     for (const QJsonValue &value : items) {
                         const QJsonObject item = value.toObject();
                         LibraryTrack track;
                         track.id = text(item, "Id");
                         if (track.id.isEmpty())
                             continue;
                         track.title = text(item, "Name");
                         track.albumId = text(item, "AlbumId");
                         if (track.albumId.isEmpty())
                             track.albumId = UnknownAlbumId;
                         track.album = text(item, "Album");
                         const QJsonArray artists = item.value(QStringLiteral("Artists")).toArray();
                         track.artist = artists.isEmpty() ? text(item, "AlbumArtist")
                                                          : artists.first().toString().left(MaxTextLength);
                         track.disc = item.value(QStringLiteral("ParentIndexNumber")).toInt();
                         track.number = item.value(QStringLiteral("IndexNumber")).toInt();
                         track.seconds = int(qint64(item.value(QStringLiteral("RunTimeTicks")).toDouble())
                                             / TicksPerSecond);
                         if (item.value(QStringLiteral("UserData")).toObject()
                                 .value(QStringLiteral("IsFavorite")).toBool())
                             m_serverFavorites.insert(track.id);
                         m_collecting.tracks.append(track);
                     }
                 },
                 [this, playlists](Jellyfin::Failure failure) {
                     if (failure != Jellyfin::Failure::None)
                         finishRefresh(failure);
                     else
                         playlists();
                 });
    };
    auto albums = [this, query, tracks] {
        Jellyfin::Query albumQuery = query;
        albumQuery.append(WithoutUserData);
        albumQuery.append({QStringLiteral("IncludeItemTypes"), QStringLiteral("MusicAlbum")});
        fetchAll(QStringLiteral("/Items"), albumQuery, MaxAlbums,
                 [this](const QJsonArray &items) {
                     for (const QJsonValue &value : items) {
                         const QJsonObject item = value.toObject();
                         LibraryAlbum album;
                         album.id = text(item, "Id");
                         if (album.id.isEmpty())
                             continue;
                         album.title = text(item, "Name");
                         album.artist = text(item, "AlbumArtist");
                         const QJsonArray albumArtists = item.value(QStringLiteral("AlbumArtists")).toArray();
                         if (!albumArtists.isEmpty())
                             album.artistId = albumArtists.first().toObject().value(QStringLiteral("Id")).toString();
                         album.year = item.value(QStringLiteral("ProductionYear")).toInt();
                         album.image = primaryImage(item, album.id);
                         m_collecting.albums.append(album);
                     }
                 },
                 [this, tracks](Jellyfin::Failure failure) {
                     if (failure != Jellyfin::Failure::None)
                         finishRefresh(failure);
                     else
                         tracks();
                 });
    };
    Jellyfin::Query artistQuery = query;
    artistQuery.append(WithoutUserData);
    fetchAll(QStringLiteral("/Artists/AlbumArtists"), artistQuery, MaxArtists,
             [this](const QJsonArray &items) {
                 for (const QJsonValue &value : items) {
                     const QJsonObject item = value.toObject();
                     LibraryArtist artist;
                     artist.id = text(item, "Id");
                     artist.name = text(item, "Name");
                     artist.image = primaryImage(item, artist.id);
                     if (!artist.id.isEmpty())
                         m_collecting.artists.append(artist);
                 }
             },
             [this, albums](Jellyfin::Failure failure) {
                 if (failure != Jellyfin::Failure::None)
                     finishRefresh(failure);
                 else
                     albums();
             });
}

void JellyfinSource::fetchPlaylistTracks(int index)
{
    if (index >= m_collecting.playlists.size()) {
        finishRefresh(Jellyfin::Failure::None);
        return;
    }
    const QString playlistId = m_collecting.playlists.at(index).id;
    fetchAll(QStringLiteral("/Playlists/%1/Items").arg(playlistId),
             {{QStringLiteral("userId"), m_config.userId},
              {QStringLiteral("EnableImages"), QStringLiteral("false")},
              {QStringLiteral("EnableUserData"), QStringLiteral("false")}},
             MaxPlaylistTracks,
             [this, index](const QJsonArray &items) {
                 for (const QJsonValue &value : items) {
                     const QJsonObject item = value.toObject();
                     // A playlist may also hold videos, which SailTune leaves out.
                     if (text(item, "Type") == QLatin1String("Audio"))
                         m_collecting.playlists[index].trackIds.append(text(item, "Id"));
                 }
             },
             [this, index](Jellyfin::Failure failure) {
                 if (failure != Jellyfin::Failure::None)
                     finishRefresh(failure);
                 else
                     fetchPlaylistTracks(index + 1);
             });
}

void JellyfinSource::finishRefresh(Jellyfin::Failure failure)
{
    setRefreshing(false);
    switch (failure) {
    case Jellyfin::Failure::None:
        setProblem(QString());
        setNeedsLogin(false);
        setReachable(true);
        m_collecting.favoritesFromSource = true;
        m_collecting.favoriteTrackIds = favoritesAfterRefresh();
        store(m_collecting);
        emit favoritesChanged();
        // A changed cover has a new tag, so a new file.
        keepCovers();
        break;
    case Jellyfin::Failure::Unauthorized:
        setReachable(true);
        setNeedsLogin(true);
        setProblem(tr("Log in again to update the library"));
        break;
    case Jellyfin::Failure::Network:
        setReachable(false);
        setProblem(tr("The server cannot be reached"));
        break;
    case Jellyfin::Failure::BadAnswer:
        setProblem(tr("The server sent an answer SailTune cannot read"));
        break;
    }
    m_collecting = LibrarySnapshot();
    m_serverFavorites.clear();
    m_favoritesDuringRefresh.clear();
}

QVector<QString> JellyfinSource::favoritesAfterRefresh() const
{
    QSet<QString> favorites = m_serverFavorites;
    QHash<QString, bool> changes = m_favoritesDuringRefresh;
    for (const QJsonValue &value : m_pending) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("action")).toString() == FavoriteAction)
            changes.insert(entry.value(QStringLiteral("id")).toString(), entry.value(QStringLiteral("value")).toBool());
    }
    for (auto change = changes.constBegin(); change != changes.constEnd(); ++change) {
        if (change.value())
            favorites.insert(change.key());
        else
            favorites.remove(change.key());
    }
    QVector<QString> ids;
    ids.reserve(favorites.size());
    for (const QString &id : favorites)
        ids.append(id);
    return ids;
}

void JellyfinSource::reportPlayed(const QString &trackId)
{
    QJsonObject entry;
    entry.insert(QStringLiteral("action"), PlayedAction);
    entry.insert(QStringLiteral("id"), trackId);
    entry.insert(QStringLiteral("date"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    addPending(entry);
}

void JellyfinSource::sendFavorite(const QString &trackId, bool favorite)
{
    if (refreshing())
        m_favoritesDuringRefresh.insert(trackId, favorite);
    // Only the last change of a track counts.
    for (int i = m_pending.size() - 1; i >= 0; --i) {
        const QJsonObject entry = m_pending.at(i).toObject();
        if (entry.value(QStringLiteral("action")).toString() == FavoriteAction
            && entry.value(QStringLiteral("id")).toString() == trackId)
            m_pending.removeAt(i);
    }
    QJsonObject entry;
    entry.insert(QStringLiteral("action"), FavoriteAction);
    entry.insert(QStringLiteral("id"), trackId);
    entry.insert(QStringLiteral("value"), favorite);
    addPending(entry);
}

void JellyfinSource::addPending(const QJsonObject &entry)
{
    if (entry.value(QStringLiteral("id")).toString().isEmpty())
        return;
    m_pending.append(entry);
    while (m_pending.size() > MaxPending)
        m_pending.removeFirst();
    savePending();
    flushPending();
}

void JellyfinSource::flushPending(const std::function<void()> &then)
{
    if (then)
        m_afterFlush.append(then);
    if (!m_flushing)
        sendNextPending();
}

void JellyfinSource::sendNextPending()
{
    if (m_pending.isEmpty() || m_token.isEmpty()) {
        finishFlush();
        return;
    }
    m_flushing = true;
    const QJsonObject entry = m_pending.first().toObject();
    const QString action = entry.value(QStringLiteral("action")).toString();
    const QString id = entry.value(QStringLiteral("id")).toString();
    QString method = QStringLiteral("POST");
    QString path;
    Jellyfin::Query query = {{QStringLiteral("userId"), m_config.userId}};
    if (action == PlayedAction) {
        path = QStringLiteral("/UserPlayedItems/%1").arg(id);
        query.append({QStringLiteral("datePlayed"), entry.value(QStringLiteral("date")).toString()});
    } else {
        path = QStringLiteral("/UserFavoriteItems/%1").arg(id);
        if (!entry.value(QStringLiteral("value")).toBool())
            method = QStringLiteral("DELETE");
    }
    Jellyfin::request(&m_network, method, Jellyfin::url(m_config.address, path, query), identity(),
                      QByteArray(), [this, entry](Jellyfin::Failure failure, const QJsonObject &) {
                          if (failure == Jellyfin::Failure::Network) {
                              setReachable(false);
                              finishFlush();
                              return;
                          }
                          if (failure == Jellyfin::Failure::Unauthorized) {
                              setNeedsLogin(true);
                              finishFlush();
                              return;
                          }
                          // Done, or refused for good, as for a track the
                          // server no longer has; either way it is not sent
                          // again. A later change of the same favorite has
                          // replaced the entry and still goes out.
                          for (int i = 0; i < m_pending.size(); ++i) {
                              if (m_pending.at(i).toObject() == entry) {
                                  m_pending.removeAt(i);
                                  break;
                              }
                          }
                          savePending();
                          sendNextPending();
                      });
}

void JellyfinSource::finishFlush()
{
    m_flushing = false;
    const QList<std::function<void()>> calls = m_afterFlush;
    m_afterFlush.clear();
    for (const std::function<void()> &call : calls)
        call();
}

void JellyfinSource::loadPending()
{
    QFile file(directory() + PendingFile);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxPendingBytes)
        return;
    const QJsonArray entries = QJsonDocument::fromJson(file.readAll()).array();
    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        const QString action = entry.value(QStringLiteral("action")).toString();
        const QString id = entry.value(QStringLiteral("id")).toString();
        // The id becomes part of a URL path.
        if ((action != PlayedAction && action != FavoriteAction) || id.isEmpty() || id.size() > 64
            || id.contains(QLatin1Char('/')) || id.contains(QLatin1Char('?')) || id.contains(QLatin1Char('#')))
            continue;
        m_pending.append(entry);
        if (m_pending.size() >= MaxPending)
            break;
    }
}

void JellyfinSource::savePending() const
{
    const QString path = directory() + PendingFile;
    if (m_pending.isEmpty()) {
        QFile::remove(path);
        return;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    file.write(QJsonDocument(m_pending).toJson(QJsonDocument::Compact));
    file.commit();
}

bool JellyfinSource::canEditPlaylists() const
{
    return true;
}

QString JellyfinSource::playlistProblem(Jellyfin::Failure failure) const
{
    switch (failure) {
    case Jellyfin::Failure::None:
        return QString();
    case Jellyfin::Failure::Network:
        return tr("The server cannot be reached");
    case Jellyfin::Failure::Unauthorized:
        return tr("Log in again to change playlists");
    case Jellyfin::Failure::BadAnswer:
        break;
    }
    return tr("The server did not change the playlist");
}

void JellyfinSource::addToPlaylist(const QString &playlistId, const QStringList &trackIds)
{
    if (playlistId.isEmpty() || trackIds.isEmpty() || trackIds.size() > MaxPlaylistTracks) {
        emit playlistEditFinished(playlistProblem(Jellyfin::Failure::BadAnswer));
        return;
    }
    Jellyfin::request(&m_network, QStringLiteral("POST"),
                      Jellyfin::url(m_config.address, QStringLiteral("/Playlists/%1/Items").arg(playlistId),
                                    {{QStringLiteral("ids"), trackIds.join(QLatin1Char(','))},
                                     {QStringLiteral("userId"), m_config.userId}}),
                      identity(), QByteArray(),
                      [this, playlistId, trackIds](Jellyfin::Failure failure, const QJsonObject &) {
                          if (failure == Jellyfin::Failure::Unauthorized)
                              setNeedsLogin(true);
                          if (failure == Jellyfin::Failure::None) {
                              library()->appendToPlaylist(playlistId, QString(), trackIds);
                              emit libraryChanged();
                          }
                          emit playlistEditFinished(playlistProblem(failure));
                      });
}

void JellyfinSource::createPlaylist(const QString &name, const QStringList &trackIds)
{
    const QString trimmed = name.trimmed().left(MaxPlaylistName);
    if (trimmed.isEmpty() || trackIds.size() > MaxPlaylistTracks) {
        emit playlistEditFinished(playlistProblem(Jellyfin::Failure::BadAnswer));
        return;
    }
    QJsonObject body;
    body.insert(QStringLiteral("Name"), trimmed);
    body.insert(QStringLiteral("Ids"), QJsonArray::fromStringList(trackIds));
    body.insert(QStringLiteral("UserId"), m_config.userId);
    body.insert(QStringLiteral("MediaType"), QStringLiteral("Audio"));
    Jellyfin::request(&m_network, QStringLiteral("POST"), Jellyfin::url(m_config.address, QStringLiteral("/Playlists")),
                      identity(), QJsonDocument(body).toJson(QJsonDocument::Compact),
                      [this, trimmed, trackIds](Jellyfin::Failure failure, const QJsonObject &answer) {
                          const QString playlistId = answer.value(QStringLiteral("Id")).toString().left(MaxTextLength);
                          if (failure == Jellyfin::Failure::None && playlistId.isEmpty())
                              failure = Jellyfin::Failure::BadAnswer;
                          if (failure == Jellyfin::Failure::Unauthorized)
                              setNeedsLogin(true);
                          if (failure == Jellyfin::Failure::None) {
                              library()->appendToPlaylist(playlistId, trimmed, trackIds);
                              emit libraryChanged();
                          }
                          emit playlistEditFinished(playlistProblem(failure));
                      });
}

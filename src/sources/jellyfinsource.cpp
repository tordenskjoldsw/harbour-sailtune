#include "jellyfinsource.h"

#include <QFile>
#include <QJsonObject>
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
    {QStringLiteral("EnableUserData"), QStringLiteral("false")},
};

} // namespace

JellyfinSource::JellyfinSource(const Config &config, const QString &directory, QObject *parent)
    : Source(config.id, directory, parent)
    , m_config(config)
{
    QFile file(directory + TokenFile);
    if (file.open(QIODevice::ReadOnly))
        m_token = QString::fromUtf8(file.readAll()).trimmed().left(MaxTextLength);
    m_needsLogin = m_token.isEmpty();
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
    const int separator = image.indexOf(QLatin1Char('|'));
    if (separator <= 0)
        return QUrl();
    return Jellyfin::url(m_config.address,
                         QStringLiteral("/Items/%1/Images/Primary").arg(image.left(separator)),
                         {{QStringLiteral("tag"), image.mid(separator + 1)},
                          {QStringLiteral("maxWidth"), QString::number(size)},
                          {QStringLiteral("quality"), QStringLiteral("90")}});
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
    Jellyfin::Query query = CommonQuery;
    query.append({QStringLiteral("userId"), m_config.userId});

    // Artists, then albums, then tracks, then playlists; each step starts
    // when the one before has every page.
    auto playlists = [this, query] {
        Jellyfin::Query playlistQuery = query;
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
        Jellyfin::Query trackQuery = query;
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
    fetchAll(QStringLiteral("/Artists/AlbumArtists"), query, MaxArtists,
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
        store(m_collecting);
        break;
    case Jellyfin::Failure::Unauthorized:
        setNeedsLogin(true);
        setProblem(tr("Log in again to update the library"));
        break;
    case Jellyfin::Failure::Network:
        setProblem(tr("The server cannot be reached"));
        break;
    case Jellyfin::Failure::BadAnswer:
        setProblem(tr("The server sent an answer SailTune cannot read"));
        break;
    }
    m_collecting = LibrarySnapshot();
}

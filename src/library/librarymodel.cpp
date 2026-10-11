#include "librarymodel.h"

#include "../sources/source.h"
#include "librarydatabase.h"

namespace {

const int ImageSize = 256;
// A search starts with the second character typed.
const int MinimumSearchLength = 2;

} // namespace

LibraryModel::LibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

Source *LibraryModel::source() const
{
    return m_source;
}

void LibraryModel::setSource(Source *source)
{
    if (m_source == source)
        return;
    if (m_source)
        disconnect(m_source, nullptr, this, nullptr);
    m_source = source;
    if (m_source)
        connect(m_source, &Source::libraryChanged, this, [this] { reload(); });
    emit sourceChanged();
    reload();
}

LibraryModel::View LibraryModel::view() const
{
    return m_view;
}

void LibraryModel::setView(View view)
{
    if (m_view == view)
        return;
    m_view = view;
    emit viewChanged();
    reload();
}

QString LibraryModel::filter() const
{
    return m_filter;
}

void LibraryModel::setFilter(const QString &filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    emit filterChanged();
    reload();
}

int LibraryModel::revision() const
{
    return m_revision;
}

void LibraryModel::setRevision(int revision)
{
    if (m_revision == revision)
        return;
    m_revision = revision;
    emit revisionChanged();
    // A finished download changes only the marks; a reset would scroll
    // the list back to the top after every file.
    const QVariantList before = m_rows;
    reload(false);
    bool sameRows = before.size() == m_rows.size();
    for (int i = 0; sameRows && i < m_rows.size(); ++i)
        sameRows = before.at(i).toMap().value(QStringLiteral("id")) == m_rows.at(i).toMap().value(QStringLiteral("id"));
    if (sameRows && !m_rows.isEmpty())
        emit dataChanged(index(0), index(m_rows.size() - 1), {DownloadedRole});
    else
        reload();
}

int LibraryModel::downloadedCount() const
{
    int count = 0;
    for (const QVariant &row : m_rows)
        count += row.toMap().value(QStringLiteral("downloaded")).toBool() ? 1 : 0;
    return count;
}

void LibraryModel::reload(bool reset)
{
    if (reset)
        beginResetModel();
    m_rows.clear();
    const LibraryDatabase *library = m_source ? m_source->library() : nullptr;
    const bool searching = m_view == SearchArtists || m_view == SearchAlbums || m_view == SearchTracks;
    if (library && !(searching && m_filter.trimmed().size() < MinimumSearchLength)) {
        switch (m_view) {
        case Artists:
            m_rows = library->artists();
            break;
        case Albums:
            m_rows = library->albums(QString());
            break;
        case ArtistAlbums:
            m_rows = library->albums(m_filter);
            break;
        case AlbumTracks:
            m_rows = library->tracksOfAlbum(m_filter);
            break;
        case Playlists:
            m_rows = library->playlists();
            break;
        case PlaylistTracks:
            m_rows = library->tracksOfPlaylist(m_filter);
            break;
        case SearchArtists:
            m_rows = library->searchArtists(m_filter);
            break;
        case SearchAlbums:
            m_rows = library->searchAlbums(m_filter);
            break;
        case SearchTracks:
            m_rows = library->searchTracks(m_filter);
            break;
        }
    }
    if (reset)
        endResetModel();
    emit countChanged();
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return QVariant();
    const QVariantMap row = m_rows.at(index.row()).toMap();
    if (role == ImageRole) {
        return m_source ? m_source->imageUrl(row.value(QStringLiteral("image")).toString(), ImageSize)
                        : QUrl();
    }
    const QByteArray key = roleNames().value(role);
    return key.isEmpty() ? QVariant() : row.value(QString::fromLatin1(key));
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {AlbumRole, "album"},
        {AlbumIdRole, "albumId"},
        {ArtistIdRole, "artistId"},
        {YearRole, "year"},
        {CountRole, "count"},
        {DiscRole, "disc"},
        {NumberRole, "number"},
        {SecondsRole, "seconds"},
        {ImageRole, "image"},
        {DownloadedRole, "downloaded"},
    };
}

QVariantList LibraryModel::rows() const
{
    return m_rows;
}

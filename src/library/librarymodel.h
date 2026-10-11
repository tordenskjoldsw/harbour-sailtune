#ifndef LIBRARYMODEL_H
#define LIBRARYMODEL_H

#include <QAbstractListModel>
#include <QPointer>
#include <QVariantList>

class Source;

// One list of a source's cached library for a page: artists, an
// artist's albums, an album's tracks, playlists, a playlist's tracks or
// search results. It reloads when the source stores a new library.
class LibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(Source *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(View view READ view WRITE setView NOTIFY viewChanged)
    // The artist, album or playlist id, or the search term.
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Bound to Downloads.revision, so the downloaded marks stay current.
    Q_PROPERTY(int revision READ revision WRITE setRevision NOTIFY revisionChanged)
    // Only what plays without a network (Sources.showingDownloadedOnly).
    Q_PROPERTY(bool downloadedOnly READ downloadedOnly WRITE setDownloadedOnly NOTIFY downloadedOnlyChanged)
    // Tracks of this list that are downloaded.
    Q_PROPERTY(int downloadedCount READ downloadedCount NOTIFY countChanged)

public:
    enum View {
        Artists,
        Albums,
        ArtistAlbums,
        AlbumTracks,
        Playlists,
        PlaylistTracks,
        SearchArtists,
        SearchAlbums,
        SearchTracks,
        FavoriteTracks
    };
    Q_ENUM(View)

    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        AlbumIdRole,
        ArtistIdRole,
        YearRole,
        CountRole,
        DiscRole,
        NumberRole,
        SecondsRole,
        ImageRole,
        DownloadedRole,
        FavoriteRole
    };

    explicit LibraryModel(QObject *parent = nullptr);

    Source *source() const;
    void setSource(Source *source);
    View view() const;
    void setView(View view);
    QString filter() const;
    void setFilter(const QString &filter);
    int revision() const;
    void setRevision(int revision);
    bool downloadedOnly() const;
    void setDownloadedOnly(bool downloadedOnly);
    int downloadedCount() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // The rows as maps, for handing a list of tracks to the player.
    Q_INVOKABLE QVariantList rows() const;

signals:
    void sourceChanged();
    void viewChanged();
    void filterChanged();
    void revisionChanged();
    void downloadedOnlyChanged();
    void countChanged();

private:
    // Without reset only the rows' content is replaced, for a caller that
    // reports the change itself.
    void reload(bool reset = true);
    // After a download or a favorite: updates the marks in place when the
    // rows stay the same, as a reset would scroll the list to the top.
    void updateMarks();

    QPointer<Source> m_source;
    View m_view = Artists;
    QString m_filter;
    int m_revision = 0;
    bool m_downloadedOnly = false;
    QVariantList m_rows;
};

#endif // LIBRARYMODEL_H

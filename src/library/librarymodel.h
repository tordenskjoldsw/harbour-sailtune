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
        SearchTracks
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
        ImageRole
    };

    explicit LibraryModel(QObject *parent = nullptr);

    Source *source() const;
    void setSource(Source *source);
    View view() const;
    void setView(View view);
    QString filter() const;
    void setFilter(const QString &filter);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // The rows as maps, for handing a list of tracks to the player.
    Q_INVOKABLE QVariantList rows() const;

signals:
    void sourceChanged();
    void viewChanged();
    void filterChanged();
    void countChanged();

private:
    void reload();

    QPointer<Source> m_source;
    View m_view = Artists;
    QString m_filter;
    QVariantList m_rows;
};

#endif // LIBRARYMODEL_H

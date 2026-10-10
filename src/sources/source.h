#ifndef SOURCE_H
#define SOURCE_H

#include <QObject>
#include <QScopedPointer>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include "../library/librarydatabase.h"

// Where music comes from: the phone itself or a server. Everything above
// this line (library pages, player, downloads) talks to a Source and
// never to a particular kind; a new server family is a new subclass.
class Source : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString kind READ kind CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)
    Q_PROPERTY(int artistCount READ artistCount NOTIFY libraryChanged)
    Q_PROPERTY(int albumCount READ albumCount NOTIFY libraryChanged)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY libraryChanged)
    Q_PROPERTY(int playlistCount READ playlistCount NOTIFY libraryChanged)

public:
    // directory: the source's own folder in the app's private storage.
    Source(const QString &id, const QString &directory, QObject *parent = nullptr);
    ~Source() override;

    QString id() const;
    virtual QString kind() const = 0;
    virtual QString name() const = 0;
    bool refreshing() const;
    QString problem() const;

    int artistCount() const;
    int albumCount() const;
    int trackCount() const;
    int playlistCount() const;

    LibraryDatabase *library() const;
    QString directory() const;

    // Reads the library from the phone's index or the server into the
    // cache; reports through refreshing, problem and libraryChanged.
    Q_INVOKABLE virtual void refresh() = 0;
    // What QtMultimedia plays for a track of the cache.
    virtual QUrl trackUrl(const QVariantMap &track) const = 0;
    // An image for QML, empty when there is none; size in pixels.
    virtual QUrl imageUrl(const QString &image, int size) const = 0;

signals:
    void nameChanged();
    void refreshingChanged();
    void problemChanged();
    void libraryChanged();

protected:
    void setRefreshing(bool refreshing);
    void setProblem(const QString &problem);
    // Stores a refresh in the cache and tells the pages.
    void store(const LibrarySnapshot &snapshot);

private:
    QString m_id;
    QString m_directory;
    QScopedPointer<LibraryDatabase> m_library;
    bool m_refreshing = false;
    QString m_problem;
};

#endif // SOURCE_H

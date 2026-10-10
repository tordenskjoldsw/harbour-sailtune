#ifndef PROBE_H
#define PROBE_H

#include <QObject>
#include <QString>
#include <QUdpSocket>
#include <QVariantList>

// The spike's three questions: can a Harbour app read the phone's music
// from the system's media index (TinySPARQL over D-Bus, permission
// MediaIndexing), find the album art the system extracted (permission
// Music), and find Jellyfin servers in the home network by UDP broadcast?
class Probe : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString report READ report NOTIFY reportChanged)
    Q_PROPERTY(QVariantList songs READ songs NOTIFY songsChanged)

public:
    explicit Probe(QObject *parent = nullptr);

    QString report() const;
    QVariantList songs() const;

    // format: 2 is JSON-LD, 0 Turtle (TrackerRdfFormat).
    Q_INVOKABLE void queryIndex(int format);
    Q_INVOKABLE void countMediaArt();
    Q_INVOKABLE void discoverServers();
    Q_INVOKABLE void clear();

signals:
    void reportChanged();
    void songsChanged();

private:
    Q_INVOKABLE void onSerialized(const QByteArray &data, int milliseconds, const QString &readError);
    void add(const QString &line);
    void parseJsonLd(const QByteArray &data);

    QString m_report;
    QVariantList m_songs;
    QUdpSocket m_udp;
};

#endif // PROBE_H

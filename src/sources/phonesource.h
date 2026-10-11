#ifndef PHONESOURCE_H
#define PHONESOURCE_H

#include <QMutex>

#include <memory>

#include "source.h"

// The music on the phone, read from the system's media index
// (TinySPARQL over D-Bus, permission MediaIndexing) with the album art
// the system extracted (permission Music). Always there, needs no login.
class PhoneSource : public Source
{
    Q_OBJECT

public:
    static const char Id[];

    PhoneSource(const QString &directory, QObject *parent = nullptr);
    ~PhoneSource() override;

    QString kind() const override;
    QString name() const override;

    void refresh() override;
    QUrl trackUrl(const QVariantMap &track) const override;
    QUrl imageUrl(const QString &image, int size) const override;

private:
    // The worker thread that reads the index hands its result to the
    // source through this; the source clears it when it goes, so a read
    // still running when the app quits finds no source.
    struct Receiver {
        QMutex mutex;
        PhoneSource *source = nullptr;
    };

    Q_INVOKABLE void onIndexRead(const QByteArray &data, const QString &error);

    std::shared_ptr<Receiver> m_receiver;
};

#endif // PHONESOURCE_H

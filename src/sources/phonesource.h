#ifndef PHONESOURCE_H
#define PHONESOURCE_H

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

    QString kind() const override;
    QString name() const override;

    void refresh() override;
    QUrl trackUrl(const QVariantMap &track) const override;
    QUrl imageUrl(const QString &image, int size) const override;

private:
    Q_INVOKABLE void onIndexRead(const QByteArray &data, const QString &error);
};

#endif // PHONESOURCE_H

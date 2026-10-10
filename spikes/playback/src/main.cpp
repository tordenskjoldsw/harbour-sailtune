#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>
#include <QString>

#include <sailfishapp.h>

#include "jellyfin.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    // Must match the Sailjail OrganizationName and ApplicationName.
    QCoreApplication::setOrganizationName(QStringLiteral("de.tordenskjold"));
    QCoreApplication::setApplicationName(QStringLiteral("musicspike"));

    Jellyfin jellyfin;

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("jellyfin"), &jellyfin);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}

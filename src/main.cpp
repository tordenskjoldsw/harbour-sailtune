#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>
#include <QString>
#include <qqml.h>

#include <sailfishapp.h>

#include "library/librarymodel.h"
#include "player/player.h"
#include "sources/source.h"
#include "sources/sources.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    // Must match the Sailjail OrganizationName and ApplicationName, which
    // decide the writable data and config directories.
    QCoreApplication::setOrganizationName(QStringLiteral("de.tordenskjold"));
    QCoreApplication::setApplicationName(QStringLiteral("sailtune"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    qmlRegisterType<LibraryModel>("harbour.sailtune", 1, 0, "LibraryModel");
    qmlRegisterUncreatableType<Source>("harbour.sailtune", 1, 0, "Source",
                                       QStringLiteral("Sources come from the app"));
    qmlRegisterUncreatableType<Sources>("harbour.sailtune", 1, 0, "Sources",
                                        QStringLiteral("One list of sources for the app"));

    Sources sources;
    Player player(&sources);

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                            QStringLiteral(APP_VERSION));
    view->rootContext()->setContextProperty(QStringLiteral("sources"), &sources);
    view->rootContext()->setContextProperty(QStringLiteral("player"), &player);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}

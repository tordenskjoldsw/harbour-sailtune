#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>
#include <QString>

#include <sailfishapp.h>

#include "probe.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    // Must match the Sailjail OrganizationName and ApplicationName.
    QCoreApplication::setOrganizationName(QStringLiteral("de.tordenskjold"));
    QCoreApplication::setApplicationName(QStringLiteral("sourcespike"));

    Probe probe;

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("probe"), &probe);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}

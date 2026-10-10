#include <QGuiApplication>
#include <QQuickView>
#include <QScopedPointer>
#include <QString>

#include <sailfishapp.h>

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QCoreApplication::setOrganizationName(QStringLiteral("de.tordenskjold"));
    QCoreApplication::setApplicationName(QStringLiteral("sailtune"));
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}

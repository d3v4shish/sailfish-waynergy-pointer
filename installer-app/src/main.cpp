#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "installercontroller.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Waynergy"));
    QCoreApplication::setApplicationName(QStringLiteral("sailfish-deskflow-setup"));
    InstallerController controller;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("installer", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}

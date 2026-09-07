#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "waynergycontroller.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Waynergy"));
    QCoreApplication::setApplicationName(QStringLiteral("sailfish-waynergy-control"));

    WaynergyController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("waynergy", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}

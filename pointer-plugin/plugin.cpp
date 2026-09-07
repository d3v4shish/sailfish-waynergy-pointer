#include "pointerfeed.h"
#include "pointercalibration.h"

#include <QQmlExtensionPlugin>
#include <qqml.h>

class WaynergyPointerPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void registerTypes(const char *uri) override
    {
        qmlRegisterType<PointerFeed>(uri, 1, 0, "PointerFeed");
        qmlRegisterType<PointerCalibration>(uri, 1, 0, "PointerCalibration");
    }
};

#include "plugin.moc"

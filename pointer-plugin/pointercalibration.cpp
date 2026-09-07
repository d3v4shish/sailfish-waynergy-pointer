#include "pointercalibration.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSettings>

namespace {
const qreal minimumScale = 0.25;
const qreal maximumScale = 2.0;
const qreal minimumXOffset = -720.0;
const qreal maximumXOffset = 720.0;
const qreal minimumYOffset = -1280.0;
const qreal maximumYOffset = 1280.0;
}

PointerCalibration::PointerCalibration(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_xScale(1.0)
    , m_yScale(1.0)
    , m_xOffset(0.0)
    , m_yOffset(0.0)
{
    connect(m_watcher, SIGNAL(fileChanged(QString)), this, SLOT(reload()));
    reload();
}

QString PointerCalibration::configPath() const
{
    return QDir::homePath() + QLatin1String("/.config/waynergy-pointer/calibration.ini");
}

bool PointerCalibration::setValues(qreal xScale, qreal yScale,
                                   qreal xOffset, qreal yOffset)
{
    xScale = qBound(minimumScale, xScale, maximumScale);
    yScale = qBound(minimumScale, yScale, maximumScale);
    xOffset = qBound(minimumXOffset, xOffset, maximumXOffset);
    yOffset = qBound(minimumYOffset, yOffset, maximumYOffset);
    if (m_xScale == xScale && m_yScale == yScale
            && m_xOffset == xOffset && m_yOffset == yOffset)
        return false;
    m_xScale = xScale;
    m_yScale = yScale;
    m_xOffset = xOffset;
    m_yOffset = yOffset;
    emit changed();
    return true;
}

void PointerCalibration::reload()
{
    const QString path = configPath();
    if (!QFileInfo::exists(path)) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSettings initial(path, QSettings::IniFormat);
        initial.setValue(QLatin1String("calibration/xScale"), 1.0);
        initial.setValue(QLatin1String("calibration/yScale"), 1.0);
        initial.setValue(QLatin1String("calibration/xOffset"), 0.0);
        initial.setValue(QLatin1String("calibration/yOffset"), 0.0);
        initial.sync();
    }
    QSettings settings(path, QSettings::IniFormat);
    const qreal xScale = settings.value(QLatin1String("calibration/xScale"), 1.0).toDouble();
    const qreal yScale = settings.value(QLatin1String("calibration/yScale"), 1.0).toDouble();
    const qreal xOffset = settings.value(QLatin1String("calibration/xOffset"), 0.0).toDouble();
    const qreal yOffset = settings.value(QLatin1String("calibration/yOffset"), 0.0).toDouble();
    setValues(xScale, yScale, xOffset, yOffset);
    if (QFileInfo::exists(path) && !m_watcher->files().contains(path))
        m_watcher->addPath(path);
}

void PointerCalibration::save()
{
    const QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(QLatin1String("calibration/xScale"), m_xScale);
    settings.setValue(QLatin1String("calibration/yScale"), m_yScale);
    settings.setValue(QLatin1String("calibration/xOffset"), m_xOffset);
    settings.setValue(QLatin1String("calibration/yOffset"), m_yOffset);
    settings.sync();
    if (QFileInfo::exists(path) && !m_watcher->files().contains(path))
        m_watcher->addPath(path);
}

void PointerCalibration::setXScale(qreal value)
{
    if (setValues(value, m_yScale, m_xOffset, m_yOffset))
        save();
}

void PointerCalibration::setYScale(qreal value)
{
    if (setValues(m_xScale, value, m_xOffset, m_yOffset))
        save();
}

void PointerCalibration::setXOffset(qreal value)
{
    if (setValues(m_xScale, m_yScale, value, m_yOffset))
        save();
}

void PointerCalibration::setYOffset(qreal value)
{
    if (setValues(m_xScale, m_yScale, m_xOffset, value))
        save();
}

void PointerCalibration::reset()
{
    if (setValues(1.0, 1.0, 0.0, 0.0))
        save();
}

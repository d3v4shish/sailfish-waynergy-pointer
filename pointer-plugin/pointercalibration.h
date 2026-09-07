#pragma once

#include <QObject>

class QFileSystemWatcher;

class PointerCalibration : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal xScale READ xScale WRITE setXScale NOTIFY changed)
    Q_PROPERTY(qreal yScale READ yScale WRITE setYScale NOTIFY changed)
    Q_PROPERTY(qreal xOffset READ xOffset WRITE setXOffset NOTIFY changed)
    Q_PROPERTY(qreal yOffset READ yOffset WRITE setYOffset NOTIFY changed)

public:
    explicit PointerCalibration(QObject *parent = nullptr);

    qreal xScale() const { return m_xScale; }
    qreal yScale() const { return m_yScale; }
    qreal xOffset() const { return m_xOffset; }
    qreal yOffset() const { return m_yOffset; }

    void setXScale(qreal value);
    void setYScale(qreal value);
    void setXOffset(qreal value);
    void setYOffset(qreal value);

    Q_INVOKABLE void reset();
    Q_INVOKABLE void reload();

signals:
    void changed();

private:
    QString configPath() const;
    void save();
    bool setValues(qreal xScale, qreal yScale, qreal xOffset, qreal yOffset);

    QFileSystemWatcher *m_watcher;
    qreal m_xScale;
    qreal m_yScale;
    qreal m_xOffset;
    qreal m_yOffset;
};

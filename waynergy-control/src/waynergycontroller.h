#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class WaynergyController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool autostart READ autostart NOTIFY stateChanged)
    Q_PROPERTY(bool servicePresent READ servicePresent NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)

public:
    explicit WaynergyController(QObject *parent = nullptr);

    bool active() const;
    bool autostart() const;
    bool servicePresent() const;
    bool busy() const;
    QString status() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart();
    Q_INVOKABLE void setAutostart(bool enabled);

signals:
    void stateChanged();

private slots:
    void commandFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum Operation {
        Idle,
        CheckActive,
        CheckEnabled,
        Start,
        Stop,
        Restart,
        Enable,
        Disable
    };

    void runSystemctl(const QStringList &arguments, Operation operation);
    void setStatusFromResult(const QString &action, int exitCode,
                             QProcess::ExitStatus exitStatus);
    QString output() const;

    QProcess *m_process;
    Operation m_operation;
    bool m_active;
    bool m_autostart;
    bool m_servicePresent;
    bool m_busy;
    QString m_status;
};

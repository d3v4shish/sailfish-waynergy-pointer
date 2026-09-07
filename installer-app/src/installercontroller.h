#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class QSocketNotifier;

class InstallerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString output READ output NOTIFY outputChanged)
    Q_PROPERTY(QString preflight READ preflight NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool passwordRequired READ passwordRequired NOTIFY stateChanged)
    Q_PROPERTY(bool activationPending READ activationPending NOTIFY stateChanged)

public:
    explicit InstallerController(QObject *parent = nullptr);
    ~InstallerController() override;

    QString phase() const;
    QString output() const;
    QString preflight() const;
    QString error() const;
    bool busy() const;
    bool passwordRequired() const;
    bool activationPending() const;

    Q_INVOKABLE void check(const QString &chrootPath);
    Q_INVOKABLE void install(const QString &chrootPath, const QString &host,
                             int port, const QString &screenName,
                             int width, int height);
    Q_INVOKABLE void restore(const QString &chrootPath);
    Q_INVOKABLE void submitPassword(QString password);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void confirmActivation();

signals:
    void stateChanged();
    void outputChanged();

private slots:
    void readPty();

private:
    bool validate(const QString &chrootPath, const QString &host, int port,
                  const QString &screenName, int width, int height);
    bool writeRequest(const QString &chrootPath, const QString &host, int port,
                      const QString &screenName, int width, int height);
    void launchRootHelper(const QStringList &arguments);
    void finishProcess();
    void appendOutput(const QString &text);
    QString applicationDataPath() const;
    QString requestPath() const;
    QString statePath() const;
    void readState();

    int m_pty;
    qint64 m_childPid;
    QSocketNotifier *m_notifier;
    QString m_phase;
    QString m_output;
    QString m_preflight;
    QString m_error;
    bool m_busy;
    bool m_passwordRequired;
    bool m_activationPending;
};

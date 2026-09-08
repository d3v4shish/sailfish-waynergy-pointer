#include "waynergycontroller.h"

#include <QProcess>
#include <QProcessEnvironment>

namespace {
const char serviceName[] = "waynergy.service";
// Sailfish OS 3.x installs systemctl in /bin.  Do not use /usr/bin here:
// on the target it does not exist, which leaves a QProcess waiting forever
// for a command that could never start.
const char systemctlPath[] = "/bin/systemctl";
const char runtimeDir[] = "/run/user/100000";
}

WaynergyController::WaynergyController(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_operation(Idle)
    , m_active(false)
    , m_autostart(false)
    , m_servicePresent(false)
    , m_busy(false)
    , m_status(QStringLiteral("Checking Waynergy service…"))
{
    connect(m_process, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(commandFinished(int,QProcess::ExitStatus)));
    connect(m_process, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(commandError(QProcess::ProcessError)));
    refresh();
}

bool WaynergyController::active() const { return m_active; }
bool WaynergyController::autostart() const { return m_autostart; }
bool WaynergyController::servicePresent() const { return m_servicePresent; }
bool WaynergyController::busy() const { return m_busy; }
QString WaynergyController::status() const { return m_status; }

void WaynergyController::runSystemctl(const QStringList &arguments,
                                      Operation operation)
{
    if (m_busy)
        return;

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (!environment.contains(QStringLiteral("XDG_RUNTIME_DIR")))
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), QLatin1String(runtimeDir));
    if (!environment.contains(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"))) {
        environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                           QLatin1String("unix:path=/run/user/100000/dbus/user_bus_socket"));
    }

    m_process->setProcessEnvironment(environment);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_operation = operation;
    m_busy = true;
    emit stateChanged();
    m_process->start(QLatin1String(systemctlPath), arguments);
}

void WaynergyController::refresh()
{
    if (m_busy)
        return;
    m_status = QStringLiteral("Checking Waynergy service…");
    runSystemctl(QStringList() << QStringLiteral("--user")
                               << QStringLiteral("is-active")
                               << QLatin1String(serviceName), CheckActive);
}

void WaynergyController::start()
{
    m_status = QStringLiteral("Starting Waynergy…");
    runSystemctl(QStringList() << QStringLiteral("--user")
                               << QStringLiteral("start")
                               << QLatin1String(serviceName), Start);
}

void WaynergyController::stop()
{
    m_status = QStringLiteral("Stopping Waynergy…");
    runSystemctl(QStringList() << QStringLiteral("--user")
                               << QStringLiteral("stop")
                               << QLatin1String(serviceName), Stop);
}

void WaynergyController::restart()
{
    m_status = QStringLiteral("Restarting Waynergy…");
    runSystemctl(QStringList() << QStringLiteral("--user")
                               << QStringLiteral("restart")
                               << QLatin1String(serviceName), Restart);
}

void WaynergyController::setAutostart(bool enabled)
{
    m_status = enabled ? QStringLiteral("Enabling Waynergy at Sailfish startup…")
                       : QStringLiteral("Disabling Waynergy at Sailfish startup…");
    runSystemctl(QStringList() << QStringLiteral("--user")
                               << (enabled ? QStringLiteral("enable")
                                           : QStringLiteral("disable"))
                               << QLatin1String(serviceName), enabled ? Enable : Disable);
}

QString WaynergyController::output() const
{
    const QString standard = QString::fromLocal8Bit(m_process->readAllStandardOutput()).trimmed();
    const QString error = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
    return !error.isEmpty() ? error : standard;
}

void WaynergyController::setStatusFromResult(const QString &action, int exitCode,
                                              QProcess::ExitStatus exitStatus)
{
    const QString detail = output();
    if (exitStatus == QProcess::NormalExit && exitCode == 0)
        m_status = detail.isEmpty() ? action + QStringLiteral(".") : action + QStringLiteral(": ") + detail;
    else
        m_status = detail.isEmpty()
                ? action + QStringLiteral(" failed (systemctl exit ") + QString::number(exitCode) + QLatin1Char(')')
                : action + QStringLiteral(" failed: ") + detail;
}

void WaynergyController::commandFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const Operation completed = m_operation;
    m_operation = Idle;
    m_busy = false;

    if (completed == CheckActive) {
        const QString state = output();
        m_servicePresent = !state.contains(QStringLiteral("not-found"))
                && !state.contains(QStringLiteral("not found"), Qt::CaseInsensitive)
                && !state.contains(QStringLiteral("could not be found"), Qt::CaseInsensitive);
        m_active = exitStatus == QProcess::NormalExit && exitCode == 0
                && state == QStringLiteral("active");
        runSystemctl(QStringList() << QStringLiteral("--user")
                                   << QStringLiteral("is-enabled")
                                   << QLatin1String(serviceName), CheckEnabled);
        return;
    }

    if (completed == CheckEnabled) {
        const QString state = output();
        m_autostart = exitStatus == QProcess::NormalExit && exitCode == 0
                && state == QStringLiteral("enabled");
        if (!m_servicePresent)
            m_status = QStringLiteral("Waynergy is not installed. Run Deskflow setup first.");
        else if (m_active)
            m_status = m_autostart ? QStringLiteral("Waynergy is running and starts after reboot.")
                                   : QStringLiteral("Waynergy is running. Autostart is off.");
        else
            m_status = m_autostart ? QStringLiteral("Waynergy is stopped but will start after reboot.")
                                   : QStringLiteral("Waynergy is stopped.");
        emit stateChanged();
        return;
    }

    switch (completed) {
    case Start: setStatusFromResult(QStringLiteral("Start"), exitCode, exitStatus); break;
    case Stop: setStatusFromResult(QStringLiteral("Stop"), exitCode, exitStatus); break;
    case Restart: setStatusFromResult(QStringLiteral("Restart"), exitCode, exitStatus); break;
    case Enable: setStatusFromResult(QStringLiteral("Autostart enabled"), exitCode, exitStatus); break;
    case Disable: setStatusFromResult(QStringLiteral("Autostart disabled"), exitCode, exitStatus); break;
    default: break;
    }
    emit stateChanged();
    refresh();
}

void WaynergyController::commandError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart || !m_busy)
        return;

    m_operation = Idle;
    m_busy = false;
    m_servicePresent = false;
    m_status = QStringLiteral("Cannot run Sailfish service manager: ")
            + m_process->errorString();
    emit stateChanged();
}

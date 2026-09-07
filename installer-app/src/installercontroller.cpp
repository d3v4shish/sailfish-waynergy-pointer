#include "installercontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSocketNotifier>
#include <QTextStream>
#include <QRegExp>
#include <QVector>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <pty.h>

namespace {
const char appId[] = "sailfish-deskflow-setup";
const char rootHelper[] = "/usr/libexec/sailfish-deskflow-setup/root-helper";
const char developerSu[] = "/usr/bin/devel-su";

bool safeToken(const QString &value, const QRegExp &expression)
{
    return expression.exactMatch(value);
}
}

InstallerController::InstallerController(QObject *parent)
    : QObject(parent)
    , m_pty(-1)
    , m_childPid(-1)
    , m_notifier(nullptr)
    , m_phase(QStringLiteral("Ready"))
    , m_busy(false)
    , m_passwordRequired(false)
    , m_activationPending(false)
{
    readState();
}

InstallerController::~InstallerController()
{
    cancel();
    if (m_notifier)
        delete m_notifier;
    if (m_pty != -1)
        ::close(m_pty);
}

QString InstallerController::phase() const { return m_phase; }
QString InstallerController::output() const { return m_output; }
QString InstallerController::preflight() const { return m_preflight; }
QString InstallerController::error() const { return m_error; }
bool InstallerController::busy() const { return m_busy; }
bool InstallerController::passwordRequired() const { return m_passwordRequired; }
bool InstallerController::activationPending() const { return m_activationPending; }

QString InstallerController::applicationDataPath() const
{
    // This must exactly match USER_STATE in root-helper.  QStandardPaths adds
    // organization-specific path components on some Sailfish Qt builds, which
    // would make the post-Lipstick-restart confirmation marker invisible.
    return QDir::homePath() + QLatin1String("/.local/share/")
            + QLatin1String(appId);
}

QString InstallerController::requestPath() const
{
    return applicationDataPath() + QLatin1String("/request.ini");
}

QString InstallerController::statePath() const
{
    return applicationDataPath() + QLatin1String("/state.ini");
}

void InstallerController::readState()
{
    QFile state(statePath());
    m_activationPending = state.open(QIODevice::ReadOnly)
            && state.readAll().contains("pending=1");
}

void InstallerController::check(const QString &chrootPath)
{
    QStringList checks;
    checks << (QFileInfo::exists(QLatin1String(developerSu))
               ? QStringLiteral("Developer mode root helper found")
               : QStringLiteral("Developer mode is not enabled"));
    checks << (QFileInfo::exists(QLatin1String("/dev/uinput"))
               ? QStringLiteral("/dev/uinput found")
               : QStringLiteral("/dev/uinput is unavailable"));
    checks << (QFileInfo::exists(chrootPath + QLatin1String("/etc/os-release"))
               ? QStringLiteral("Chroot found: %1").arg(chrootPath)
               : QStringLiteral("Chroot will be downloaded: %1").arg(chrootPath));
    checks << QStringLiteral("Target: Sailfish OS 3.2 armv7hl only");
    m_preflight = checks.join(QLatin1Char('\n'));
    m_error.clear();
    emit stateChanged();
}

bool InstallerController::validate(const QString &chrootPath, const QString &host,
                                   int port, const QString &screenName,
                                   int width, int height)
{
    const QRegExp chrootRx(QStringLiteral("^/home/nemo/chroots/[A-Za-z0-9._/-]+$"));
    const QRegExp hostRx(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9.-]*$"));
    const QRegExp screenRx(QStringLiteral("^[A-Za-z0-9._-]{1,64}$"));
    if (!safeToken(chrootPath, chrootRx) || chrootPath.contains(QStringLiteral("..")))
        m_error = QStringLiteral("Chroot path must stay below /home/nemo/chroots/");
    else if (!safeToken(host, hostRx))
        m_error = QStringLiteral("Server must be a hostname or IPv4 address without spaces");
    else if (!safeToken(screenName, screenRx))
        m_error = QStringLiteral("Screen name may contain only letters, digits, dot, dash, and underscore");
    else if (port < 1 || port > 65535)
        m_error = QStringLiteral("Port must be between 1 and 65535");
    else if (width < 1 || width > 8192 || height < 1 || height > 8192)
        m_error = QStringLiteral("Display dimensions must be between 1 and 8192 pixels");
    else
        return true;
    emit stateChanged();
    return false;
}

bool InstallerController::writeRequest(const QString &chrootPath, const QString &host,
                                       int port, const QString &screenName,
                                       int width, int height)
{
    QDir().mkpath(applicationDataPath());
    QSaveFile file(requestPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_error = QStringLiteral("Cannot write installer request: %1").arg(file.errorString());
        emit stateChanged();
        return false;
    }
    QTextStream stream(&file);
    stream << "chroot=" << chrootPath << '\n'
           << "host=" << host << '\n'
           << "port=" << port << '\n'
           << "screen=" << screenName << '\n'
           << "width=" << width << '\n'
           << "height=" << height << '\n';
    if (!file.commit()) {
        m_error = QStringLiteral("Cannot save installer request");
        emit stateChanged();
        return false;
    }
    QFile::setPermissions(requestPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

void InstallerController::install(const QString &chrootPath, const QString &host,
                                  int port, const QString &screenName,
                                  int width, int height)
{
    if (m_busy || !validate(chrootPath, host, port, screenName, width, height)
            || !writeRequest(chrootPath, host, port, screenName, width, height))
        return;
    launchRootHelper(QStringList() << QStringLiteral("--install")
                                   << QStringLiteral("--config") << requestPath());
}

void InstallerController::restore(const QString &chrootPath)
{
    if (m_busy || !validate(chrootPath, QStringLiteral("localhost"), 24800,
                             QStringLiteral("sailfish"), 720, 1280)
            || !writeRequest(chrootPath, QStringLiteral("localhost"), 24800,
                              QStringLiteral("sailfish"), 720, 1280))
        return;
    launchRootHelper(QStringList() << QStringLiteral("--restore")
                                   << QStringLiteral("--config") << requestPath());
}

void InstallerController::launchRootHelper(const QStringList &arguments)
{
    if (!QFileInfo::isExecutable(QLatin1String(developerSu))
            || !QFileInfo::isExecutable(QLatin1String(rootHelper))) {
        m_error = QStringLiteral("Installer files or developer mode are unavailable");
        emit stateChanged();
        return;
    }

    int master = -1;
    const pid_t pid = ::forkpty(&master, nullptr, nullptr, nullptr);
    if (pid == -1) {
        m_error = QStringLiteral("Cannot start developer-mode helper: %1")
                .arg(QString::fromLocal8Bit(std::strerror(errno)));
        emit stateChanged();
        return;
    }
    if (pid == 0) {
        QList<QByteArray> storage;
        storage << QByteArray("devel-su") << QByteArray(rootHelper);
        for (const QString &argument : arguments)
            storage << argument.toLocal8Bit();
        QVector<char *> argv;
        for (QByteArray &item : storage)
            argv << item.data();
        argv << nullptr;
        ::execv(developerSu, argv.data());
        _exit(127);
    }

    m_pty = master;
    m_childPid = pid;
    m_busy = true;
    m_passwordRequired = false;
    m_error.clear();
    m_output.clear();
    m_phase = QStringLiteral("Waiting for developer password");
    m_notifier = new QSocketNotifier(m_pty, QSocketNotifier::Read, this);
    connect(m_notifier, SIGNAL(activated(int)), this, SLOT(readPty()));
    emit outputChanged();
    emit stateChanged();
}

void InstallerController::appendOutput(const QString &text)
{
    m_output += text;
    if (m_output.size() > 65536)
        m_output.remove(0, m_output.size() - 65536);
    if (text.contains(QStringLiteral("Password:"))) {
        m_passwordRequired = true;
        m_phase = QStringLiteral("Developer password required");
    }
    if (text.contains(QStringLiteral("PHASE="))) {
        const int start = text.lastIndexOf(QStringLiteral("PHASE=")) + 6;
        const int end = text.indexOf(QLatin1Char('\n'), start);
        m_phase = text.mid(start, end < 0 ? -1 : end - start).trimmed();
    }
    if (text.contains(QStringLiteral("RESULT=PENDING_CONFIRMATION"))) {
        m_activationPending = true;
        m_phase = QStringLiteral("Verify pointer before rollback deadline");
    }
    if (text.contains(QStringLiteral("RESULT=SUCCESS")))
        m_phase = QStringLiteral("Installation complete");
    if (text.contains(QStringLiteral("RESULT=RESTORED"))) {
        m_activationPending = false;
        m_phase = QStringLiteral("Restored");
    }
    emit outputChanged();
    emit stateChanged();
}

void InstallerController::readPty()
{
    char buffer[4096];
    const ssize_t received = ::read(m_pty, buffer, sizeof(buffer));
    if (received > 0) {
        appendOutput(QString::fromLocal8Bit(buffer, received));
        return;
    }
    if (received == -1 && errno == EINTR)
        return;
    finishProcess();
}

void InstallerController::finishProcess()
{
    if (m_notifier) {
        delete m_notifier;
        m_notifier = nullptr;
    }
    if (m_pty != -1) {
        ::close(m_pty);
        m_pty = -1;
    }
    int status = 0;
    if (m_childPid > 0) {
        while (::waitpid(static_cast<pid_t>(m_childPid), &status, 0) == -1
               && errno == EINTR) {
        }
    }
    m_childPid = -1;
    m_busy = false;
    m_passwordRequired = false;
    if (!m_output.contains(QStringLiteral("RESULT=")) && m_error.isEmpty())
        m_error = QStringLiteral("Root helper ended before reporting completion");
    else if (m_output.contains(QStringLiteral("RESULT=FAILED")) && m_error.isEmpty())
        m_error = QStringLiteral("Installer did not complete; see the log below");
    readState();
    emit stateChanged();
}

void InstallerController::submitPassword(QString password)
{
    if (m_pty == -1 || !m_passwordRequired)
        return;
    QByteArray bytes = password.toLocal8Bit();
    bytes.append('\n');
    ::write(m_pty, bytes.constData(), static_cast<size_t>(bytes.size()));
    std::fill(bytes.begin(), bytes.end(), '\0');
    password.fill(QChar(0));
    m_passwordRequired = false;
    m_phase = QStringLiteral("Running privileged installer");
    emit stateChanged();
}

void InstallerController::cancel()
{
    if (m_childPid > 0) {
        m_phase = QStringLiteral("Cancelling");
        ::kill(static_cast<pid_t>(m_childPid), SIGTERM);
        emit stateChanged();
    }
}

void InstallerController::confirmActivation()
{
    QDir().mkpath(applicationDataPath());
    QSaveFile marker(applicationDataPath() + QLatin1String("/activation-confirmed"));
    if (!marker.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_error = QStringLiteral("Cannot record activation confirmation");
        emit stateChanged();
        return;
    }
    marker.write("confirmed\n");
    if (!marker.commit()) {
        m_error = QStringLiteral("Cannot save activation confirmation");
        emit stateChanged();
        return;
    }
    QFile::setPermissions(marker.fileName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    m_activationPending = false;
    m_phase = QStringLiteral("Confirmation recorded; safety timer will finalize");
    emit stateChanged();
}

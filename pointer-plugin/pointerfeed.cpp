#include "pointerfeed.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWindow>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
constexpr char socketPath[] = "/run/display/waynergy-pointer.sock";
constexpr std::uint32_t magic = 0x57505452U; // "WPTR"
constexpr std::uint16_t version = 1U;
constexpr std::uint16_t resyncFlag = 1U;

struct Packet {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t reserved;
    std::uint32_t width;
    std::uint32_t height;
    std::int32_t x;
    std::int32_t y;
};
static_assert(sizeof(Packet) == 24, "pointer packet layout changed");
}

PointerFeed::PointerFeed(QObject *parent)
    : QObject(parent)
    , m_fd(-1)
    , m_notifier(nullptr)
    , m_hideTimer(new QTimer(this))
    , m_x(0)
    , m_y(0)
    , m_sourceWidth(720)
    , m_sourceHeight(1280)
    , m_actualX(0)
    , m_actualY(0)
    , m_actualSourceWidth(0)
    , m_actualSourceHeight(0)
    , m_actualPositionValid(false)
    , m_visible(false)
    , m_enabled(true)
    , m_bound(false)
{
    // The QPA plugin reports the real virtual-mouse location to Lipstick as
    // QMouseEvents.  Watching that position keeps the indicator in precisely
    // the same coordinate space as clicks, even if a surface change has
    // clamped the relative mouse device.
    if (QCoreApplication::instance())
        QCoreApplication::instance()->installEventFilter(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(2000);
    connect(m_hideTimer, SIGNAL(timeout()), this, SLOT(hide()));
    bindSocket();
}

PointerFeed::~PointerFeed()
{
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeEventFilter(this);
    closeSocket();
}

bool PointerFeed::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseMove
            || event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonRelease) {
        const QMouseEvent *mouseEvent = static_cast<const QMouseEvent *>(event);
        const QPointF point = mouseEvent->windowPos();
        const QWindow *window = qobject_cast<const QWindow *>(watched);
        const qreal sourceWidth = window ? window->width() : m_actualSourceWidth;
        const qreal sourceHeight = window ? window->height() : m_actualSourceHeight;
        if (!m_actualPositionValid || m_actualX != point.x() || m_actualY != point.y()
                || m_actualSourceWidth != sourceWidth || m_actualSourceHeight != sourceHeight) {
            m_actualX = point.x();
            m_actualY = point.y();
            m_actualSourceWidth = sourceWidth;
            m_actualSourceHeight = sourceHeight;
            m_actualPositionValid = true;
            emit actualPositionChanged();
        }
    }
    return false;
}

void PointerFeed::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (!m_enabled)
        hide();
    emit enabledChanged();
}

void PointerFeed::refresh()
{
    if (!m_enabled || !m_visible)
        return;
    emit positionChanged();
    emit sourceSizeChanged();
    m_hideTimer->start();
}

bool PointerFeed::bindSocket()
{
    struct sockaddr_un address;
    struct stat status;

    if (::lstat(socketPath, &status) == 0) {
        if (status.st_uid != ::geteuid()) {
            qWarning() << "Waynergy pointer socket is owned by another user";
            return false;
        }
        if (::unlink(socketPath) != 0) {
            qWarning() << "Cannot remove stale Waynergy pointer socket:" << strerror(errno);
            return false;
        }
    } else if (errno != ENOENT) {
        qWarning() << "Cannot inspect Waynergy pointer socket:" << strerror(errno);
        return false;
    }

    m_fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (m_fd == -1) {
        qWarning() << "Cannot create Waynergy pointer socket:" << strerror(errno);
        return false;
    }
    std::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, socketPath, sizeof(address.sun_path) - 1);
    if (::bind(m_fd, reinterpret_cast<struct sockaddr *>(&address),
               offsetof(struct sockaddr_un, sun_path) + sizeof(socketPath)) != 0) {
        qWarning() << "Cannot bind Waynergy pointer socket:" << strerror(errno);
        closeSocket();
        return false;
    }
    if (::chmod(socketPath, S_IRUSR | S_IWUSR) != 0)
        qWarning() << "Cannot protect Waynergy pointer socket:" << strerror(errno);

    m_bound = true;
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, SIGNAL(activated(int)), this, SLOT(readPackets()));
    return true;
}

void PointerFeed::closeSocket()
{
    if (m_notifier) {
        delete m_notifier;
        m_notifier = nullptr;
    }
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
    if (m_bound && ::unlink(socketPath) != 0 && errno != ENOENT)
        qWarning() << "Cannot remove Waynergy pointer socket:" << strerror(errno);
    m_bound = false;
}

void PointerFeed::readPackets()
{
    Packet packet;
    ssize_t bytes;
    bool positionChanged = false;
    bool sizeChanged = false;
    bool resync = false;

    while ((bytes = ::recv(m_fd, &packet, sizeof(packet), MSG_DONTWAIT)) > 0) {
        if (bytes != sizeof(packet) || packet.magic != magic || packet.version != version
                || (packet.reserved & ~resyncFlag)
                || packet.width == 0 || packet.height == 0
                || packet.width > 8192 || packet.height > 8192
                || packet.x < 0 || packet.y < 0
                || packet.x >= static_cast<std::int32_t>(packet.width)
                || packet.y >= static_cast<std::int32_t>(packet.height)) {
            continue;
        }
        if (m_x != packet.x || m_y != packet.y)
            positionChanged = true;
        if (m_sourceWidth != packet.width || m_sourceHeight != packet.height)
            sizeChanged = true;
        if (packet.reserved & resyncFlag)
            resync = true;
        m_x = packet.x;
        m_y = packet.y;
        m_sourceWidth = packet.width;
        m_sourceHeight = packet.height;
    }
    if (bytes == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
        qWarning() << "Cannot read Waynergy pointer socket:" << strerror(errno);
    if (positionChanged || resync)
        emit this->positionChanged();
    if (sizeChanged || resync)
        emit sourceSizeChanged();
    if (resync)
        emit resyncRequested();
    if (positionChanged || sizeChanged || resync) {
        setVisible(m_enabled);
        if (m_enabled)
            m_hideTimer->start();
    }
}

void PointerFeed::hide()
{
    m_hideTimer->stop();
    setVisible(false);
}

void PointerFeed::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    emit visibleChanged();
}

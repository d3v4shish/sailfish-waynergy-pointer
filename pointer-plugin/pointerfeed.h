#pragma once

#include <QObject>

class QSocketNotifier;
class QTimer;

class PointerFeed : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal x READ x NOTIFY positionChanged)
    Q_PROPERTY(qreal y READ y NOTIFY positionChanged)
    Q_PROPERTY(qreal sourceWidth READ sourceWidth NOTIFY sourceSizeChanged)
    Q_PROPERTY(qreal sourceHeight READ sourceHeight NOTIFY sourceSizeChanged)
    // The coordinate which Qt's evdev mouse handler actually delivered to
    // Lipstick.  This is intentionally separate from the Deskflow packet:
    // the packet is absolute, while Sailfish's virtual mouse is relative and
    // may be clamped when a window is replaced.
    Q_PROPERTY(qreal actualX READ actualX NOTIFY actualPositionChanged)
    Q_PROPERTY(qreal actualY READ actualY NOTIFY actualPositionChanged)
    Q_PROPERTY(qreal actualSourceWidth READ actualSourceWidth NOTIFY actualPositionChanged)
    Q_PROPERTY(qreal actualSourceHeight READ actualSourceHeight NOTIFY actualPositionChanged)
    Q_PROPERTY(bool actualPositionValid READ actualPositionValid NOTIFY actualPositionChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit PointerFeed(QObject *parent = nullptr);
    ~PointerFeed() override;

    qreal x() const { return m_x; }
    qreal y() const { return m_y; }
    qreal sourceWidth() const { return m_sourceWidth; }
    qreal sourceHeight() const { return m_sourceHeight; }
    qreal actualX() const { return m_actualX; }
    qreal actualY() const { return m_actualY; }
    qreal actualSourceWidth() const { return m_actualSourceWidth; }
    qreal actualSourceHeight() const { return m_actualSourceHeight; }
    bool actualPositionValid() const { return m_actualPositionValid; }
    bool visible() const { return m_visible; }
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    Q_INVOKABLE void refresh();

signals:
    void positionChanged();
    void sourceSizeChanged();
    void actualPositionChanged();
    void visibleChanged();
    void enabledChanged();
    void resyncRequested();

private slots:
    void readPackets();
    void hide();

private:
    bool bindSocket();
    void closeSocket();
    void setVisible(bool visible);
    bool eventFilter(QObject *watched, QEvent *event) override;

    int m_fd;
    QSocketNotifier *m_notifier;
    QTimer *m_hideTimer;
    qreal m_x;
    qreal m_y;
    qreal m_sourceWidth;
    qreal m_sourceHeight;
    qreal m_actualX;
    qreal m_actualY;
    qreal m_actualSourceWidth;
    qreal m_actualSourceHeight;
    bool m_actualPositionValid;
    bool m_visible;
    bool m_enabled;
    bool m_bound;
};

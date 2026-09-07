import QtQuick 2.2
import Waynergy.Pointer 1.0

Item {
    id: overlay

    property bool locked: false
    property bool allocated: false
    property real screenWidth: 0
    property real screenHeight: 0
    readonly property int dotSize: 28
    readonly property bool pointerActive: feed.visible && !locked

    function resyncAfterWindowClose() {
        if (!pointerActive)
            return
        calibration.reload()
        allocated = true
        fadeTimer.stop()
        feed.refresh()
    }

    PointerFeed {
        id: feed
        enabled: !overlay.locked
    }

    PointerCalibration {
        id: calibration
    }

    Connections {
        target: feed
        onVisibleChanged: {
            if (feed.visible) {
                fadeTimer.stop()
                overlay.allocated = true
            } else {
                fadeTimer.restart()
            }
        }

        onResyncRequested: {
            if (!overlay.locked) {
                calibration.reload()
                overlay.allocated = true
                fadeTimer.stop()
            }
        }
    }

    width: allocated ? screenWidth : 0
    height: allocated ? screenHeight : 0
    visible: allocated
    opacity: feed.visible ? 1.0 : 0.0
    z: 100000

    Behavior on opacity {
        NumberAnimation { duration: 150 }
    }

    Timer {
        id: fadeTimer
        interval: 150
        repeat: false
        onTriggered: overlay.allocated = false
    }

    Item {
        id: dot
        width: overlay.dotSize
        height: overlay.dotSize
        x: Math.max(0, Math.min(overlay.width - width,
                                (feed.actualPositionValid
                                 ? feed.actualX * (feed.actualSourceWidth > 0
                                                   ? overlay.width / feed.actualSourceWidth : 1)
                                 : feed.x * overlay.width / feed.sourceWidth) * calibration.xScale
                                + calibration.xOffset - width / 2))
        y: Math.max(0, Math.min(overlay.height - height,
                                (feed.actualPositionValid
                                 ? feed.actualY * (feed.actualSourceHeight > 0
                                                   ? overlay.height / feed.actualSourceHeight : 1)
                                 : feed.y * overlay.height / feed.sourceHeight) * calibration.yScale
                                + calibration.yOffset - height / 2))

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "#cc000000"
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 8
            height: width
            radius: width / 2
            color: "#f2ffffff"
        }
    }
}

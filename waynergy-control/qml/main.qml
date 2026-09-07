import QtQuick 2.2
import Sailfish.Silica 1.0

ApplicationWindow {
    initialPage: Component {
        Page {
            id: page
            allowedOrientations: Orientation.All

            SilicaFlickable {
                anchors.fill: parent
                contentHeight: content.height + Theme.paddingLarge

                PullDownMenu {
                    MenuItem {
                        text: "Refresh status"
                        enabled: !waynergy.busy
                        onClicked: waynergy.refresh()
                    }
                }

                Column {
                    id: content
                    width: parent.width
                    spacing: Theme.paddingMedium

                    PageHeader { title: "Waynergy control" }

                    Label {
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        x: Theme.horizontalPageMargin
                        wrapMode: Text.Wrap
                        color: Theme.secondaryColor
                        text: "Controls the installed Waynergy user service. Enable autostart to run it again when Sailfish starts after a reboot."
                    }

                    SectionHeader { text: "Service status" }

                    Label {
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        x: Theme.horizontalPageMargin
                        text: waynergy.active ? "Running" : "Stopped"
                        color: waynergy.active ? Theme.highlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeLarge
                    }

                    Label {
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        x: Theme.horizontalPageMargin
                        wrapMode: Text.Wrap
                        text: waynergy.status
                        color: Theme.secondaryColor
                    }

                    TextSwitch {
                        width: parent.width
                        text: "Start automatically after reboot"
                        description: "Enables the Waynergy user service in systemd"
                        checked: waynergy.autostart
                        enabled: waynergy.servicePresent && !waynergy.busy
                        onClicked: waynergy.setAutostart(checked)
                    }

                    SectionHeader { text: "Actions" }

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Start"
                        enabled: waynergy.servicePresent && !waynergy.active && !waynergy.busy
                        onClicked: waynergy.start()
                    }

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Restart"
                        enabled: waynergy.servicePresent && !waynergy.busy
                        onClicked: waynergy.restart()
                    }

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Stop"
                        enabled: waynergy.servicePresent && waynergy.active && !waynergy.busy
                        onClicked: waynergy.stop()
                    }

                    BusyIndicator {
                        anchors.horizontalCenter: parent.horizontalCenter
                        size: BusyIndicatorSize.Medium
                        running: waynergy.busy
                        visible: running
                    }

                    Label {
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        x: Theme.horizontalPageMargin
                        visible: !waynergy.servicePresent && !waynergy.busy
                        wrapMode: Text.Wrap
                        color: Theme.errorColor
                        text: "Install Waynergy with Deskflow setup before using this control app."
                    }
                }

                VerticalScrollDecorator { flickable: parent }
            }
        }
    }
}

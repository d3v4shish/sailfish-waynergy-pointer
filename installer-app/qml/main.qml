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
                    MenuItem { text: "Recheck prerequisites"; onClicked: installer.check(chrootPath.text) }
                }

                Column {
                    id: content
                    width: parent.width
                    spacing: Theme.paddingMedium

                    PageHeader { title: "Deskflow setup" }

                    Label {
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        x: Theme.horizontalPageMargin
                        text: "Installs the Sailfish pointer integration. Developer mode is required; your password is sent only to devel-su and is not saved."
                        color: Theme.secondaryColor
                        wrapMode: Text.Wrap
                    }

                    TextField { id: chrootPath; width: parent.width; label: "Debian chroot"; text: "/home/nemo/chroots/deskflow-bookworm" }
                    TextField { id: host; width: parent.width; label: "Deskflow server"; text: "192.168.0.102"; inputMethodHints: Qt.ImhUrlCharactersOnly }
                    TextField { id: port; width: parent.width; label: "Port"; text: "24800"; inputMethodHints: Qt.ImhDigitsOnly }
                    TextField { id: screen; width: parent.width; label: "Client screen name"; text: "sailfish" }

                    Row {
                        width: parent.width
                        TextField { id: widthField; width: parent.width / 2; label: "Width"; text: "720"; inputMethodHints: Qt.ImhDigitsOnly }
                        TextField { id: heightField; width: parent.width / 2; label: "Height"; text: "1280"; inputMethodHints: Qt.ImhDigitsOnly }
                    }

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: installer.busy ? "Installing…" : "Install / update"
                        enabled: !installer.busy
                        onClicked: installer.install(chrootPath.text, host.text, Number(port.text), screen.text, Number(widthField.text), Number(heightField.text))
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Restore previous setup"
                        enabled: !installer.busy
                        onClicked: installer.restore(chrootPath.text)
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Cancel current operation"
                        enabled: installer.busy
                        onClicked: installer.cancel()
                    }

                    TextField {
                        id: password
                        width: parent.width
                        visible: installer.passwordRequired
                        label: "Developer password"
                        echoMode: TextInput.Password
                        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                        onAccepted: { installer.submitPassword(text); text = "" }
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: installer.passwordRequired
                        text: "Continue"
                        onClicked: { installer.submitPassword(password.text); password.text = "" }
                    }

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: installer.activationPending
                        text: "Pointer and UI work — keep setup"
                        onClicked: installer.confirmActivation()
                    }

                    SectionHeader { text: "Status" }
                    Label { width: parent.width - 2 * Theme.horizontalPageMargin; x: Theme.horizontalPageMargin; text: installer.phase; wrapMode: Text.Wrap }
                    Label { width: parent.width - 2 * Theme.horizontalPageMargin; x: Theme.horizontalPageMargin; text: installer.preflight; color: Theme.secondaryColor; wrapMode: Text.Wrap }
                    Label { width: parent.width - 2 * Theme.horizontalPageMargin; x: Theme.horizontalPageMargin; visible: installer.error.length > 0; text: installer.error; color: Theme.errorColor; wrapMode: Text.Wrap }
                    TextArea { width: parent.width; height: Theme.itemSizeLarge * 4; readOnly: true; text: installer.output; label: "Installer log"; wrapMode: TextEdit.WrapAnywhere }
                }
            }
            Component.onCompleted: installer.check(chrootPath.text)
        }
    }
}

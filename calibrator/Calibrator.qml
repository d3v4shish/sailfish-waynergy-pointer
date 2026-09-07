import QtQuick 2.2
import Sailfish.Silica 1.0
import Waynergy.Pointer 1.0

ApplicationWindow {
    initialPage: Component {
        Page {
            id: page
            allowedOrientations: Orientation.All

            PointerCalibration {
                id: calibration
            }

            SilicaFlickable {
                anchors.fill: parent
                contentHeight: controls.height + Theme.paddingLarge

                PullDownMenu {
                    MenuItem {
                        text: "Reset calibration"
                        onClicked: calibration.reset()
                    }
                }

                Column {
                    id: controls
                    width: parent.width
                    spacing: Theme.paddingMedium

                    PageHeader {
                        title: "Pointer calibration"
                    }

                    Label {
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        x: Theme.horizontalPageMargin
                        text: "Move the Deskflow cursor over the phone, then adjust these values until the dot lines up. Changes apply immediately and are kept after restart."
                        color: Theme.secondaryColor
                        wrapMode: Text.Wrap
                    }

                    SectionHeader { text: "Position" }

                    Slider {
                        width: parent.width
                        minimumValue: -720
                        maximumValue: 720
                        stepSize: 1
                        label: "Horizontal offset"
                        value: calibration.xOffset
                        valueText: Math.round(value) + " px"
                        onValueChanged: calibration.xOffset = value
                    }

                    Slider {
                        width: parent.width
                        minimumValue: -1280
                        maximumValue: 1280
                        stepSize: 1
                        label: "Vertical offset"
                        value: calibration.yOffset
                        valueText: Math.round(value) + " px"
                        onValueChanged: calibration.yOffset = value
                    }

                    SectionHeader { text: "Travel range" }

                    Slider {
                        width: parent.width
                        minimumValue: 0.25
                        maximumValue: 2.0
                        stepSize: 0.01
                        label: "Horizontal scale"
                        value: calibration.xScale
                        valueText: Math.round(value * 100) + "%"
                        onValueChanged: calibration.xScale = value
                    }

                    Slider {
                        width: parent.width
                        minimumValue: 0.25
                        maximumValue: 2.0
                        stepSize: 0.01
                        label: "Vertical scale"
                        value: calibration.yScale
                        valueText: Math.round(value * 100) + "%"
                        onValueChanged: calibration.yScale = value
                    }

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Reset to 100% / 0 px"
                        onClicked: calibration.reset()
                    }

                    Label {
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        x: Theme.horizontalPageMargin
                        text: "Tip: first correct the offsets near the top-left. Then move to the opposite edge and correct the scales."
                        color: Theme.secondaryColor
                        wrapMode: Text.Wrap
                    }
                }

                VerticalScrollDecorator { flickable: parent }
            }
        }
    }
}

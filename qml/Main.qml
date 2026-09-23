import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Window

Window {
    id: root

    width: 860
    height: 620
    minimumWidth: 860
    minimumHeight: 620
    maximumWidth: 860
    maximumHeight: 620
    visible: true
    title: "DS4 Lightbar"
    color: "#0a2133"

    onClosing: function(close) {
        close.accepted = false
        root.hide()
    }

    property color surface: "#0d2b42"
    property color panel: "#082236"
    property color panelSoft: "#102f47"
    property color line: "#265a7c"
    property color softText: "#83a9c1"
    property color strongText: "#e9f6ff"
    property color cyan: "#3fd6ff"
    property color violet: "#805cff"

    Rectangle {
        anchors.fill: parent
        color: root.surface
    }

    Rectangle {
        id: header
        height: 58
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: root.panelSoft

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: "#2b5b7a"
        }

        Rectangle {
            id: logoBox
            width: 34
            height: 34
            radius: 8
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            color: "#1f5071"
            border.color: root.cyan

            Image {
                anchors.centerIn: parent
                width: 25
                height: 25
                source: "qrc:/qt/qml/DS4Lightbar/assets/icon.png"
                fillMode: Image.PreserveAspectFit
            }
        }

        Column {
            anchors.left: logoBox.right
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Text {
                text: "DS4 Lightbar"
                color: root.strongText
                font.pixelSize: 16
            }

            Text {
                text: "DUALSHOCK 4 LIGHTBAR CONTROL"
                color: root.softText
                font.pixelSize: 11
            }
        }

        Rectangle {
            id: statusPill
            width: Math.min(300, statusLabel.implicitWidth + 40)
            height: 30
            radius: 5
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            color: "#082238"
            border.color: "#2e6385"

            Rectangle {
                width: 8
                height: 8
                radius: 4
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                color: lightbar.controllerConnected ? "#44e6a0" : "#f07864"
            }

            Text {
                id: statusLabel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 28
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                color: root.strongText
                font.pixelSize: 12
                elide: Text.ElideRight
                text: lightbar.controllerConnected
                    ? lightbar.controllerName + " - " + (lightbar.batteryPercent >= 0 ? lightbar.batteryPercent + "%" : "--%")
                    : "Controller disconnected"
            }
        }
    }

    Item {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom

        Column {
            id: mainColumn
            width: Math.min(780, content.width - 44)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 18
            spacing: 10

            RowLayout {
                width: parent.width
                spacing: 18

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "Lightbar"
                        color: root.strongText
                        font.pixelSize: 21
                    }

                    Text {
                        text: "Static color, smooth spectrum cycle, or lightbar off"
                        color: root.softText
                        font.pixelSize: 13
                    }
                }

                Button {
                    id: logButton

                    text: "Open log"
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    Layout.preferredWidth: 104
                    Layout.preferredHeight: 32
                    onClicked: lightbar.showLogWindow()

                    contentItem: Text {
                        text: logButton.text
                        color: root.strongText
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 10
                        color: logButton.down ? "#214f70" : logButton.hovered ? "#1b4665" : "#133854"
                        border.color: logButton.activeFocus ? root.cyan : "#356f92"
                        border.width: 1

                        Behavior on color {
                            ColorAnimation { duration: 110 }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 128
                radius: 8
                color: root.panel
                border.color: root.line

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.top: parent.top
                    anchors.topMargin: 14
                    text: "MODE / " + lightbar.modeName.toUpperCase()
                    color: root.softText
                    font.pixelSize: 13
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.top: parent.top
                    anchors.topMargin: 14
                    text: lightbar.currentColor
                    color: root.softText
                    font.pixelSize: 11
                }

                Item {
                    id: preview
                    width: 290
                    height: 78
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 12

                    Shape {
                        id: glowShape
                        anchors.centerIn: parent
                        width: parent.width + 54
                        height: parent.height + 30
                        opacity: lightbar.controllerConnected ? 0.32 : 0.18
                        layer.enabled: true
                        layer.smooth: true

                        Behavior on opacity {
                            NumberAnimation { duration: 180 }
                        }

                        ShapePath {
                            fillColor: lightbar.currentColor
                            strokeWidth: 0
                            startX: glowShape.width * 0.11
                            startY: glowShape.height * 0.30

                            PathLine { x: glowShape.width * 0.89; y: glowShape.height * 0.30 }
                            PathCubic {
                                control1X: glowShape.width * 0.96
                                control1Y: glowShape.height * 0.30
                                control2X: glowShape.width * 0.98
                                control2Y: glowShape.height * 0.42
                                x: glowShape.width * 0.90
                                y: glowShape.height * 0.52
                            }
                            PathLine { x: glowShape.width * 0.61; y: glowShape.height * 0.67 }
                            PathCubic {
                                control1X: glowShape.width * 0.56
                                control1Y: glowShape.height * 0.72
                                control2X: glowShape.width * 0.44
                                control2Y: glowShape.height * 0.72
                                x: glowShape.width * 0.39
                                y: glowShape.height * 0.67
                            }
                            PathLine { x: glowShape.width * 0.10; y: glowShape.height * 0.52 }
                            PathCubic {
                                control1X: glowShape.width * 0.02
                                control1Y: glowShape.height * 0.42
                                control2X: glowShape.width * 0.04
                                control2Y: glowShape.height * 0.30
                                x: glowShape.width * 0.11
                                y: glowShape.height * 0.30
                            }
                        }
                    }

                    Shape {
                        id: lightbarShape
                        anchors.fill: parent
                        layer.enabled: true
                        layer.smooth: true

                        ShapePath {
                            fillColor: lightbar.currentColor
                            strokeColor: "#bcecff"
                            strokeWidth: 1.4
                            startX: lightbarShape.width * 0.11
                            startY: lightbarShape.height * 0.24

                            PathLine { x: lightbarShape.width * 0.89; y: lightbarShape.height * 0.24 }
                            PathCubic {
                                control1X: lightbarShape.width * 0.95
                                control1Y: lightbarShape.height * 0.24
                                control2X: lightbarShape.width * 0.97
                                control2Y: lightbarShape.height * 0.36
                                x: lightbarShape.width * 0.88
                                y: lightbarShape.height * 0.48
                            }
                            PathLine { x: lightbarShape.width * 0.61; y: lightbarShape.height * 0.61 }
                            PathCubic {
                                control1X: lightbarShape.width * 0.56
                                control1Y: lightbarShape.height * 0.66
                                control2X: lightbarShape.width * 0.44
                                control2Y: lightbarShape.height * 0.66
                                x: lightbarShape.width * 0.40
                                y: lightbarShape.height * 0.61
                            }
                            PathLine { x: lightbarShape.width * 0.12; y: lightbarShape.height * 0.48 }
                            PathCubic {
                                control1X: lightbarShape.width * 0.03
                                control1Y: lightbarShape.height * 0.36
                                control2X: lightbarShape.width * 0.05
                                control2Y: lightbarShape.height * 0.24
                                x: lightbarShape.width * 0.11
                                y: lightbarShape.height * 0.24
                            }
                        }

                        ShapePath {
                            fillColor: "#26ffffff"
                            strokeWidth: 0
                            startX: lightbarShape.width * 0.14
                            startY: lightbarShape.height * 0.24

                            PathLine { x: lightbarShape.width * 0.86; y: lightbarShape.height * 0.24 }
                            PathLine { x: lightbarShape.width * 0.82; y: lightbarShape.height * 0.34 }
                            PathLine { x: lightbarShape.width * 0.18; y: lightbarShape.height * 0.34 }
                            PathLine { x: lightbarShape.width * 0.14; y: lightbarShape.height * 0.24 }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: lightbar.modeIndex === 0 ? 290 : 218
                radius: 8
                color: root.panel
                border.color: root.line

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        width: parent.width
                        spacing: 16

                        Text {
                            text: "Light behavior"
                            color: root.strongText
                            font.pixelSize: 14
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            id: modeCombo

                            model: lightbar.modeNames
                            currentIndex: lightbar.modeIndex
                            Layout.preferredWidth: 196
                            Layout.preferredHeight: 36
                            onActivated: function(index) {
                                lightbar.modeIndex = index
                            }

                            contentItem: Item {
                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 36
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modeCombo.displayText
                                    color: root.strongText
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            indicator: Canvas {
                                id: modeArrow

                                width: 12
                                height: 8
                                x: modeCombo.width - width - 14
                                y: modeCombo.height / 2 - height / 2

                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.lineWidth = 2
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"
                                    ctx.strokeStyle = root.softText
                                    ctx.beginPath()
                                    ctx.moveTo(1, 1)
                                    ctx.lineTo(width / 2, height - 1)
                                    ctx.lineTo(width - 1, 1)
                                    ctx.stroke()
                                }
                            }

                            background: Rectangle {
                                radius: 10
                                color: modeCombo.down ? "#214f70" : modeCombo.hovered ? "#1b4665" : "#133854"
                                border.color: modeCombo.activeFocus ? root.cyan : "#356f92"
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation { duration: 110 }
                                }
                            }

                            popup: Popup {
                                y: modeCombo.height + 6
                                width: modeCombo.width
                                implicitHeight: contentItem.implicitHeight + 8
                                padding: 4

                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: contentHeight
                                    model: modeCombo.popup.visible ? modeCombo.delegateModel : null
                                    currentIndex: modeCombo.highlightedIndex
                                }

                                background: Rectangle {
                                    radius: 10
                                    color: "#0b263b"
                                    border.color: "#356f92"
                                    border.width: 1
                                }
                            }

                            delegate: ItemDelegate {
                                width: modeCombo.width
                                height: 34
                                highlighted: modeCombo.highlightedIndex === index

                                contentItem: Text {
                                    text: modelData
                                    color: highlighted ? root.strongText : "#c7ddea"
                                    font.pixelSize: 12
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                background: Rectangle {
                                    radius: 8
                                    color: highlighted ? "#1b4665" : "transparent"
                                }
                            }
                        }
                    }

                    ControlRow {
                        title: "LED intensity"
                        locked: lightbar.modeIndex === 2
                        value: lightbar.intensity
                        suffix: "%"
                        from: 0
                        to: 100
                        onValueEdited: function(value) {
                            lightbar.intensity = value
                        }
                    }

                    ControlRow {
                        title: "Transition speed"
                        locked: lightbar.modeIndex === 2 || (lightbar.modeIndex === 0 && lightbar.staticNoPulse)
                        value: lightbar.speedMs
                        suffix: " ms"
                        from: 5
                        to: 120
                        onValueEdited: function(value) {
                            lightbar.speedMs = value
                        }
                    }

                    Loader {
                        width: parent.width
                        height: lightbar.modeIndex === 0 ? 58 : 0
                        active: lightbar.modeIndex === 0
                        sourceComponent: staticColorConfig
                    }
                }
            }
        }
    }

    Window {
        id: logWindow
        width: 680
        height: 420
        title: "DS4 Lightbar - Runtime Log"
        color: "#071a29"
        visible: lightbar.logWindowVisible

        onClosing: function(close) {
            close.accepted = false
            lightbar.logWindowVisible = false
        }

        Rectangle {
            anchors.fill: parent
            color: "#071a29"

            Text {
                id: logTitle
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 22
                anchors.topMargin: 20
                text: "Runtime log"
                color: root.strongText
                font.pixelSize: 20
            }

            ListView {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: logTitle.bottom
                anchors.bottom: parent.bottom
                anchors.margins: 22
                model: lightbar.logs
                clip: true

                delegate: Text {
                    width: ListView.view.width
                    text: modelData
                    color: "#c6d8e5"
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    lineHeight: 1.25
                }

                onCountChanged: positionViewAtEnd()
            }
        }
    }

    Component {
        id: staticColorConfig

        ColumnLayout {
            width: parent ? parent.width : 740
            height: parent ? parent.height : 58
            spacing: 10

            CheckBox {
                id: staticNoPulseBox

                Layout.fillWidth: true
                Layout.preferredHeight: 24
                checked: lightbar.staticNoPulse
                text: "Keep light static"
                onToggled: lightbar.staticNoPulse = checked

                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    x: 0
                    y: staticNoPulseBox.height / 2 - height / 2
                    radius: 4
                    color: staticNoPulseBox.checked ? root.cyan : "#061b2b"
                    border.color: staticNoPulseBox.checked ? "#bcecff" : "#386b8d"

                    Rectangle {
                        anchors.centerIn: parent
                        width: 8
                        height: 8
                        radius: 2
                        color: "#082236"
                        visible: staticNoPulseBox.checked
                    }
                }

                contentItem: Text {
                    text: staticNoPulseBox.text
                    color: root.strongText
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 28
                }
            }

            ColorPicker {
                Layout.fillWidth: true
                Layout.preferredHeight: 24
            }
        }
    }

    component ColorPicker: Item {
        id: picker

        width: parent ? parent.width : 740
        height: 24
        property real hue: 0.54

        function clamp(value, minValue, maxValue) {
            return Math.max(minValue, Math.min(maxValue, value))
        }

        function applyHue(px) {
            hue = clamp(px / Math.max(1, hueBar.width), 0, 1)
            lightbar.staticColor = Qt.hsva(hue, 1, 1, 1)
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 9

            Item {
                id: hueBar
                Layout.fillWidth: true
                Layout.preferredHeight: 16

                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    border.color: "#386b8d"
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#ff0000" }
                        GradientStop { position: 0.17; color: "#ffff00" }
                        GradientStop { position: 0.34; color: "#00ff00" }
                        GradientStop { position: 0.50; color: "#00ffff" }
                        GradientStop { position: 0.67; color: "#0000ff" }
                        GradientStop { position: 0.84; color: "#ff00ff" }
                        GradientStop { position: 1.0; color: "#ff0000" }
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        preventStealing: true
                        onPressed: function(mouse) { picker.applyHue(mouse.x) }
                        onClicked: function(mouse) { picker.applyHue(mouse.x) }
                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                picker.applyHue(mouse.x)
                            }
                        }
                    }
                }

                Rectangle {
                    width: 10
                    height: 22
                    radius: 5
                    x: picker.hue * hueBar.width - width / 2
                    y: -3
                    color: "#e9f6ff"
                    border.color: "#0a2133"
                }
            }
        }
    }

    component ControlRow: Item {
        id: row

        property string title
        property int value
        property int from
        property int to
        property string suffix
        property bool locked: false
        signal valueEdited(int value)

        width: parent ? parent.width : 740
        height: 52
        enabled: !locked
        opacity: locked ? 0.42 : 1.0

        Behavior on opacity {
            NumberAnimation { duration: 120 }
        }

        function isSpeedControl() {
            return String(row.title).toLowerCase().indexOf("speed") >= 0
        }

        function commitText() {
            if (row.locked) {
                valueInput.text = row.value.toString()
                return
            }

            var parsed = parseInt(valueInput.text)

            if (isNaN(parsed)) {
                valueInput.text = row.value.toString()
                return
            }

            valueEdited(Math.max(row.from, Math.min(row.to, parsed)))
        }

        onValueChanged: {
            if (!valueInput.activeFocus) {
                valueInput.text = row.value.toString()
            }
        }

        Item {
            anchors.left: parent.left
            anchors.right: valueInput.left
            anchors.rightMargin: 18
            height: valueInput.height
            anchors.top: parent.top

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: row.title
                color: root.strongText
                font.pixelSize: 14
            }
        }

        TextField {
            id: valueInput
            width: 76
            height: 30
            anchors.right: parent.right
            anchors.top: parent.top
            text: row.value.toString()
            color: root.strongText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            selectByMouse: true
            validator: IntValidator { bottom: row.from; top: row.to }
            onAccepted: row.commitText()
            onEditingFinished: row.commitText()

            background: Rectangle {
                radius: 4
                color: "#173d5b"
                border.color: "#386b8d"
            }
        }

        Text {
            anchors.right: valueInput.left
            anchors.rightMargin: 8
            anchors.verticalCenter: valueInput.verticalCenter
            text: row.suffix
            color: root.softText
            font.pixelSize: 12
        }

        Slider {
            id: slider
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 32
            from: row.from
            to: row.to
            stepSize: 1
            value: row.value
            wheelEnabled: true
            onMoved: {
                if (!row.locked) {
                    row.valueEdited(Math.round(value))
                }
            }

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: 6
                radius: 4
                color: "#051c2d"

                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: row.isSpeedControl() ? root.violet : root.cyan
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: 16
                height: 16
                radius: 9
                color: "#dff8ff"
                border.width: 3
                border.color: row.isSpeedControl() ? root.violet : root.cyan
            }
        }
    }
}

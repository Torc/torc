import QtQuick 2.0

Item {
    id: checkbox
    property bool enabled: false

    signal clicked (var event)
    Keys.onEnterPressed: clicked()
    Keys.onReturnPressed: clicked()
    property alias mouseArea: mousearea
    property bool pressed: mousearea.pressed
    property bool hovered: mousearea.containsMouse

    onClicked: enabled = !enabled

    MouseArea {
        id: mousearea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: checkbox.clicked(mouse);
    }
}

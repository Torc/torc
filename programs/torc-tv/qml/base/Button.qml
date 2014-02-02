import QtQuick 2.0

FocusScope {
    id: button
    signal clicked (var event)
    Keys.onEnterPressed: clicked(undefined)
    Keys.onReturnPressed: clicked(undefined)
    property alias mouseArea: mousearea
    property bool pressed: mousearea.pressed
    property bool hovered: mousearea.containsMouse

    MouseArea {
        id: mousearea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: button.clicked(mouse)
    }
}

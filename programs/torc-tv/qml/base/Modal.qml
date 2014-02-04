import QtQuick 2.0

Rectangle {
    id: modal
    color: 'black'
    opacity: 0.5
    z: 100
    visible: false

    property string title
    property string message
    property alias mouseArea: mousearea

    MouseArea {
        id: mousearea
        anchors.fill: parent
        hoverEnabled: true
    }
}

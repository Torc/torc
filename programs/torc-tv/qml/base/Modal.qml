import QtQuick 2.0

FocusScope {
    id: modal
    z: 100
    visible: false

    property alias mouseArea: mousearea

    MouseArea {
        id: mousearea
        anchors.fill: parent
        hoverEnabled: true
    }
}

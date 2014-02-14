import QtQuick 2.0
import "base"

Modal {
    id: modal
    anchors.fill: window

    Rectangle {
        id: background
        anchors.fill: parent
        opacity: visible ? 0.7 : 0.0
        color: 'black'
        Behavior on opacity { PropertyAnimation { duration: 75} }
    }
}

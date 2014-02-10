import QtQuick 2.0
import "base"

Modal {
    id: modal
    anchors.fill: window

    Rectangle {
        id: background
        anchors.fill: parent
        opacity: 0.75
        color: 'darkgray'
    }
}

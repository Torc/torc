import QtQuick 2.0
import "base"

Button {
    id: button
    property alias image: image
    property alias text:  text
    property int   minimumWidth: 10
    property int   maxmimumWidth: 200
    property int   minimumHeight: 10
    property int   maximumHeight: 100
    property int   maximumLineCount: 2
    property color basecolor: '#d9d9d9'

    width: text.contentWidth + 20
    height: text.contentHeight + 20

    Rectangle {
        id: image
        anchors.fill: parent
        radius: 5
        gradient: Gradient {
            GradientStop { position: 0; color: 'white' }
            GradientStop { position: 0.5; color: basecolor }
        }
        border.color: 'lightgray'
        border.width: 1
        visible: false
    }

    Dropshadow {
        source: image
        color: (hovered | activeFocus) ? "#aa000000" : dropcolor
        verticalOffset: (hovered | activeFocus) ? 6 : 3
        opacity: button.pressed ? 0.75 : 1.0
        focus: true
    }

    Text {
        id: text
        color: '#111111'
        font.family: 'Arial'
        font.pointSize: 25
        font.weight: Font.Light
        anchors.centerIn: parent
        wrapMode: Text.Wrap
        maximumLineCount: button.maximumLineCount
        clip: true

        onContentSizeChanged: {
            width = Math.max(button.minimumWidth, Math.min(button.maxmimumWidth, width))
            height = Math.max(button.minimumHeight, Math.min(button.maximumHeight, height))
        }
    }
}

import QtQuick 2.0
import "base"

Button {
    id: button
    property alias image: image
    property alias text:  text
    property bool  shrinkToFit: true
    property int   minimumWidth: 10
    property int   maxmimumWidth: 200
    property int   minimumHeight: 10
    property int   maximumHeight: 100
    property int   maximumLineCount: 2
    property int   padding: 20
    property color basecolor: '#d9d9d9'

    width: text.contentWidth + padding
    height: text.contentHeight + padding

    Rectangle {
        id: image
        anchors.fill: parent
        radius: 10
        gradient: Gradient {
            GradientStop { position: 0; color: 'white' }
            GradientStop { position: 0.5; color: basecolor }
        }
        border.color: 'white'
        border.width: 2
        visible: false
    }

    Dropshadow {
        source: image
        color: (hovered | activeFocus) ? "#aa000000" : dropcolor
        verticalOffset: (hovered | activeFocus) ? 6 : 3
        opacity: pressed ? 0.75 : 1.0
        focus: true
    }

    TextBase {
        id: text
        maximumLineCount: button.maximumLineCount
        clip: true

        onContentSizeChanged: {
            button.width = Math.max(minimumWidth, Math.min(maxmimumWidth, contentWidth)) + padding
            button.height = Math.max(minimumHeight, Math.min(maximumHeight, contentHeight)) + padding

            if (shrinkToFit && contentWidth > maxmimumWidth) { font.pointSize -= 1; }
        }
    }
}

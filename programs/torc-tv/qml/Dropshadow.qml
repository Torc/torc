import QtQuick 2.0
import QtGraphicalEffects 1.0

DropShadow {
    property color dropcolor: '#aaaaaaaa'

    anchors.fill: source
    verticalOffset: 2
    radius: 5.0
    samples: 16
    spread: 0.0
    color: dropcolor;
    transparentBorder: true
    cached: true
}

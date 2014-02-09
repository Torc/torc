import QtQuick 2.0

Text {
    id: icon
    anchors.centerIn: parent
    color: '#111111'
    font.pointSize: 35
    font.family: webfont.name
    text: checkbox.enabled ? "\uf05d" : "\uf10c"
}

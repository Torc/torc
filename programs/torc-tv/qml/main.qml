import QtQuick 2.0
import Torc.Core 0.1
import Torc.Media 0.1
import Torc.QML 0.1

Rectangle {
    width: 1920
    height: 1080
    color: "black"

    FontLoader {
        id: fontLoader
        source: "fontawesome-webfont.ttf"
    }

    Keys.onPressed: {
        var key = event.key
        if (key == Qt.Key_Escape)
            Qt.quit()
        else if (key == Qt.Key_F1)
            if (torcPower.GetCanSuspend()) torcPower.Suspend()
    }

    TorcQMLMediaElement {
        anchors.fill: parent
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        font.pointSize: 30
        text: torcFPS.framesPerSecond.toFixed(1)
        color: "white"
    }

    ListModel {
        id: mainMenuModel

        ListElement {
            menutext: "\uf011"
        }
        ListElement {
            menutext: "\uf008"
        }
        ListElement {
            menutext: "\uf03a"
        }
        ListElement {
            menutext: "\uf002"
        }
        ListElement {
            menutext: "\uf001"
        }
        ListElement {
            menutext: "\uf0e4"
        }
        ListElement {
            menutext: "\uf030"
        }
        ListElement {
            menutext: "\uf007"
        }
        ListElement {
            menutext: "\uf085"
        }
    }

    ListView
    {
        id: mainMenuView
        focus: true
        spacing: 20
        width: parent.width
        height: 200
        anchors.bottom: parent.bottom
        model: mainMenuModel
        orientation: ListView.Horizontal
        preferredHighlightBegin: width / 2 - 100
        preferredHighlightEnd: width / 2
        highlightRangeMode: "StrictlyEnforceRange"
        highlightMoveVelocity: 1000

        delegate: Text {
            anchors.verticalCenter: parent.verticalCenter
            font.pointSize: 200
            font.family: fontLoader.name
            text: menutext
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            scale:  ListView.isCurrentItem ? 1.0 : 0.9
            color: ListView.isCurrentItem ? "#ddffffff" : "#400000ff"
            Behavior on scale { PropertyAnimation{} }
            Behavior on color { PropertyAnimation{} }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    mainMenuView.currentIndex = index
                }
            }
        }
    }
}

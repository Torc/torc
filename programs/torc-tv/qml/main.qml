import QtQuick 2.0
import Torc.Core 0.1
import Torc.Media 0.1

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

    Text {
        id: background
        anchors.fill: parent
        font.family: fontLoader.name
        font.pointSize: 700
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: "\uf011"
        color: "#60707070"
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        font.pointSize: 70
        text: torcFPS.framesPerSecond
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

        onCurrentItemChanged: background.text = model.get(currentIndex).menutext

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

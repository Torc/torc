import QtQuick 2.1
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0

ApplicationWindow {
    id: window
    width: 600
    height: 400
    title: qsTr("torc-desktop")

    Component.onCompleted: window.title = TorcLocalContext.GetUuid()

    TabView {
        id: frame
        tabPosition: Qt.TopEdge
        anchors.fill: parent
        anchors.margins: Qt.platform.os === "osx" ? 12 : 2

        Tab {
            id: controlPage
            title: qsTr("Player")
        }

        Tab {
            title: qsTr("Media")
        }

        Tab {
            title: qsTr("Settings")

            SplitView {
                id: settings
                anchors.fill: parent
                anchors.topMargin: 8
                orientation:  Qt.Horizontal

                ScrollView {
                    ListView {
                        id: settingsGroup
                        width: 150

                        delegate: Button {
                            width: parent.width
                            text: modelData.m_uiName
                            enabled: modelData.rowCount() > 0
                        }

                        model: RootSetting
                    }
                }

                ScrollView {
                    id: settingsView
                    Text {
                        text: "Empty"
                    }
                }
            }
        }
    }
}

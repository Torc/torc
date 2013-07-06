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
            TableView {
                id: mediaView
                anchors.fill: parent
                anchors.margins: 8
                model: TorcMediaMaster

                TableViewColumn {
                    width: 250
                    title: qsTr("Name")
                    role: "name"
                }

                TableViewColumn {
                    width: 350
                    title: qsTr("Location")
                    role: "url"
                }
            }
        }

        Tab {
            title: qsTr("Status")

            SplitView {
                anchors.fill: parent
                anchors.margins: 8
                orientation: Qt.Vertical

                property var haveDevices: false

                GroupBox {
                    id: noTorcDevices
                    visible: !haveDevices && TorcNetworkedContext
                    anchors.margins: 8
                    Layout.fillWidth: true
                    title: qsTr("No other Torc devices detected on this network")
                }

                GroupBox {
                    id: torcDevices
                    visible: haveDevices && TorcNetworkedContext
                    anchors.margins: 8
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Other Torc devices on this network")

                    TableView {
                        id: torcDevicesView
                        anchors.fill: parent
                        onRowCountChanged: haveDevices = rowCount > 0

                        model: TorcNetworkedContext

                        TableViewColumn {
                            width: 250
                            title: qsTr("Name")
                            role: "m_name"
                        }

                        TableViewColumn {
                            width: 200
                            title: qsTr("Address")
                            role: "m_uiAddress"
                        }

                        TableViewColumn {
                            width: 300
                            title: qsTr("Identifier")
                            role: "m_uuid"
                        }
                    }
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Other interesting stuff")
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Really interesting stuff")
                }
            }
        }

        Tab {
            title: qsTr("Settings")

            SplitView {
                id: settings
                anchors.fill: parent
                anchors.margins: 8
                orientation: Qt.Horizontal

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

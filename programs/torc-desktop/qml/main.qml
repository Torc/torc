import QtQuick 2.1
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0
import Torc.Core 0.1
import Torc.Media 0.1
import Torc.QML 0.1

ApplicationWindow {
    id: window
    width: 800
    height: 600
    title: "torc-desktop"

    Component.onCompleted: window.title = TorcLocalContext.GetUuid()

    TabView {
        id: frame
        tabPosition: Qt.TopEdge
        anchors.fill: parent
        anchors.margins: Qt.platform.os === "osx" ? 12 : 2

        Tab {
            title: qsTranslate("TorcMediaMaster", "Media")

            SplitView {
                anchors.fill: parent
                anchors.margins: 8
                orientation: Qt.Vertical

                TorcQMLMediaElement {
                    width: 600
                    height: 400
                }

                Column {
                    TableView {
                        id: mediaView
                        model: TorcMediaMasterFilter {
                            id: mediaFilter
                            sourceModel: TorcMediaMaster
                        }

                        sortIndicatorVisible: true

                        height: parent.height - 34
                        anchors.right: parent.right
                        anchors.left: parent.left

                        onSortIndicatorOrderChanged: mediaFilter.SetSortOrder(mediaView.sortIndicatorOrder, mediaView.sortIndicatorColumn)
                        onSortIndicatorColumnChanged: mediaFilter.SetSortOrder(mediaView.sortIndicatorOrder, mediaView.sortIndicatorColumn)

                        TableViewColumn {
                            width: 250
                            title: qsTranslate("TorcMediaMaster", "Name")
                            role: "name"
                        }

                        TableViewColumn {
                            width: 350
                            title: qsTranslate("TorcMediaMaster", "Location")
                            role: "url"
                        }
                    }

                    Row {
                        spacing: 12
                        Text {
                            text: qsTranslate("TorcMediaMaster", "Search")
                        }

                        TextField {
                            text: mediaFilter.textFilter
                            onTextChanged: mediaFilter.SetTextFilter(text)
                        }

                        Button {
                            text: qsTranslate("TorcMediaMaster", "Search using")
                            menu: Menu {
                                MenuItem {
                                    text: qsTranslate("TorcMediaMaster", "Name")
                                    checkable: true
                                    checked: mediaFilter.filterByName
                                    onTriggered: mediaFilter.filterByName = true
                                }
                                MenuItem {
                                    text: qsTranslate("TorcMediaMaster", "Location")
                                    checkable: true
                                    checked: !mediaFilter.filterByName
                                    onTriggered: mediaFilter.filterByName = false
                                }
                            }
                        }

                        CheckBox{
                            anchors.leftMargin: 16
                            id: videoCheckbox
                            checked: true
                            text: qsTranslate("TorcMediaMaster", "Videos")
                            onClicked: mediaFilter.SetMediaTypeFilter(TorcMedia.Video, checked)
                        }
                        CheckBox{
                            id: musicCheckbox
                            checked: true
                            text: qsTranslate("TorcMediaMaster", "Music")
                            onClicked: mediaFilter.SetMediaTypeFilter(TorcMedia.Audio, checked)
                        }
                        CheckBox{
                            id: photosCheckbox
                            checked: true
                            text: qsTranslate("TorcMediaMaster", "Photos")
                            onClicked: mediaFilter.SetMediaTypeFilter(TorcMedia.Image, checked)
                        }
                    }
                }
            }
        }

        Tab {
            title: qsTranslate("TorcNetworkedContext", "Status")

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
                    title: qsTranslate("TorcNetworkedContext", "No other Torc devices discovered")
                }

                GroupBox {
                    id: torcDevices
                    visible: haveDevices && TorcNetworkedContext
                    anchors.margins: 8
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTranslate("TorcNetworkedContext", "Other Torc devices on this network")

                    TableView {
                        id: torcDevicesView
                        anchors.fill: parent
                        onRowCountChanged: haveDevices = rowCount > 0

                        model: TorcNetworkedContext

                        TableViewColumn {
                            width: 250
                            title: qsTranslate("TorcNetworkedContext", "Name")
                            role: "name"
                        }

                        TableViewColumn {
                            width: 200
                            title: qsTranslate("TorcNetworkedContext", "Address")
                            role: "uiAddress"
                        }

                        TableViewColumn {
                            width: 100
                            title: qsTranslate("TorcNetworkedContext", "API Version")
                            role: "apiVersion"
                        }

                        TableViewColumn {
                            width: 100
                            title: qsTranslate("TorcNetworkedContext", "Connected")
                            role: "connected"
                        }
                    }
                }
            }
        }

        Tab {
            title: qsTranslate("TorcSetting", "Settings")

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

import QtQuick 2.0

Item {
    id: topMenu
    anchors.fill: parent

    ListModel { id: topMenuModel }

    Component.onCompleted: {
        topMenuModel.append({
            menuIcon: "\uf011",
            menuText: qsTranslate('TorcPower', 'Power'),
            menuSource: "Power"
        });
        topMenuModel.append({
            menuIcon: "\uf008",
            menuText: qsTranslate('TorcMedia', 'Media'),
            menuSource: "Media"
        });
    }

    // this is needed to filter hover events from the media player
    //MouseArea {
    //    anchors.fill: topMenuView
    //    hoverEnabled: true
    //}

    ListView
    {
        id: topMenuView
        model: topMenuModel
        focus: true
        spacing: 5
        width: 1280
        height: 720
        anchors.left: parent.left
        anchors.leftMargin: 50
        orientation: ListView.Vertical
        verticalLayoutDirection: ListView.TopToBottom
        preferredHighlightBegin: (height / 2) - 100
        preferredHighlightEnd: (height / 2) + 100
        highlightRangeMode: "StrictlyEnforceRange"
        highlightMoveVelocity: 500

        delegate: Component {
            id: component
            Loader {
                id: loader
                sourceComponent: Component {
                    ButtonImage {
                        id: menuButton
                        minimumHeight: 40
                        minimumWidth: 150
                        maxmimumWidth: 150
                        anchors.horizontalCenter: parent.horizontalCenter
                        text.text:  menuText
                        focus: loader.ListView.isCurrentItem;
                        Behavior on scale { PropertyAnimation{ duration: 100 } }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: topMenuView.currentIndex = index
                        }

                        onClicked: {
                            menuLoader.setSource('')
                            menuLoader.setSource(menuSource + '.qml')
                        }

                        Keys.onRightPressed: clicked(undefined)

                        Loader { id: menuLoader }

                        Connections {
                            target: menuLoader.item
                            onDeleted: {
                                menuButton.forceActiveFocus();
                                menuLoader.setSource('');
                            }
                        }
                    }
                }
            }
        }
    }
}

import QtQuick 2.0

FocusScope {
    id: powerMenu
    ListModel { id: powerModel }
    anchors.fill: parent

    Keys.onPressed: {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Left) {
            event.accepted = true;
            deleted();
        } else if (event.key === Qt.Key_Right) {
            event.accepted = true;
        }
    }

    Component.onCompleted: {
        powerModel.append({
            menuText: qsTranslate('TorcPower', 'Suspend'),
            confirm: qsTranslate('TorcPower', 'Are you sure you want to suspend the device?')
        });
        powerModel.append({
            menuText: qsTranslate('TorcPower', 'Shutdown'),
            confirm: qsTranslate('TorcPower', 'Are you sure you want to shutdown the device?')
        });
        powerModel.append({
            menuText: qsTranslate('TorcPower', 'Hibernate'),
            confirm: qsTranslate('TorcPower', 'Are you sure you want to hibernate the device?')
        });
        powerModel.append({
            menuText: qsTranslate('TorcPower', 'Restart'),
            confirm: qsTranslate('TorcPower', 'Are you sure you want to restart the device?')
        });
    }

    ListView {
        id: powerView
        width: 200
        height: 500
        model: powerModel
        spacing: 5
        focus: true
        opacity: 0
        Behavior on opacity { PropertyAnimation {} }

        Component.onCompleted: {
            forceActiveFocus()
            opacity = 1.0
        }

        delegate: ButtonImage {
            minimumHeight: 40
            minimumWidth: 150
            maxmimumWidth: 150
            anchors.horizontalCenter: parent.horizontalCenter
            text.text: menuText
            focus: loader.ListView.isCurrentItem;
            Behavior on scale { PropertyAnimation{ duration: 100 } }
            MouseArea {
                anchors.fill: parent
                onClicked: powerView.currentIndex = index
            }
        }
    }
}

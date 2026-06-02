import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    id: appRoot
    visible: true
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 560
    title: appController.sessionOpen ? qsTr("ДокументПро") : qsTr("Вход — ДокументПро")

    Loader {
        anchors.fill: parent
        sourceComponent: (authController.isAuthenticated && appController.sessionOpen) ? mainView : authView
    }

    Component {
        id: mainView
        Main {}
    }

    Component {
        id: authView
        LoginPage {}
    }
}

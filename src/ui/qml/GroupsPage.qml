import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: page
    signal openGroup(string groupId, string groupNumber)

    background: Rectangle { color: "#eceef2" }

    property alias groupsListModel: groupsList

    ListModel { id: groupsList }

    function refresh() {
        var m = appController.groupsModel
        groupsList.clear()
        if (!m || typeof m.count !== "function")
            return
        for (var i = 0; i < m.count(); ++i) {
            var g = m.itemAt(i)
            if (g && g.groupId)
                groupsList.append(g)
        }
    }

    header: ToolBar {
        background: Rectangle { color: "#ffffff" }
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            Label {
                text: qsTr("Учебные группы")
                font.bold: true
                font.pixelSize: 18
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("+ Группа")
                highlighted: true
                onClicked: addGroupDialog.open()
            }
        }
    }

    ListView {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10
        clip: true
        model: groupsList

        delegate: Rectangle {
            width: ListView.view.width
            height: 64
            radius: 10
            color: "#ffffff"
            border.color: "#e5e7eb"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                Label {
                    text: number
                    font.bold: true
                    font.pixelSize: 16
                    Layout.fillWidth: true
                }
                Button {
                    text: qsTr("Открыть")
                    onClicked: page.openGroup(groupId, number)
                }
                Button {
                    text: "✕"
                    flat: true
                    onClicked: {
                        deleteConfirm.groupId = groupId
                        deleteConfirm.open()
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                z: -1
                onClicked: page.openGroup(groupId, number)
            }
        }

        Label {
            anchors.centerIn: parent
            visible: groupsList.count === 0
            text: qsTr("Нет групп. Нажмите «+ Группа».")
            color: "#6b7280"
        }
    }

    Dialog {
        id: addGroupDialog
        title: qsTr("Новая группа")
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        anchors.centerIn: parent
        width: 360

        ColumnLayout {
            spacing: 8
            Label { text: qsTr("Номер группы") }
            TextField {
                id: groupNumberField
                Layout.fillWidth: true
                placeholderText: qsTr("например, ИВТ-101")
            }
        }
        onAccepted: {
            if (appController.createGroup(groupNumberField.text))
                groupNumberField.text = ""
            page.refresh()
        }
    }

    Dialog {
        id: deleteConfirm
        property string groupId: ""
        title: qsTr("Удалить группу?")
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        Label { text: qsTr("Будут удалены все студенты и ведомости этой группы.") }
        onAccepted: {
            appController.deleteGroup(deleteConfirm.groupId)
            page.refresh()
        }
    }

    Component.onCompleted: refresh()
    Connections {
        target: appController
        function onGroupsChanged() { page.refresh() }
    }
}

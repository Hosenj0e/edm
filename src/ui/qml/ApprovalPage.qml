import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: page
    property var stackView: null
    signal back()
    signal openSheet(string sheetId, string groupId, string groupNumber)

    background: Rectangle { color: "#eceef2" }

    ListModel { id: pendingList }

    function refresh() {
        pendingList.clear()
        var arr = appController.pendingSheetsList()
        for (var i = 0; i < arr.length; ++i)
            pendingList.append(arr[i])
    }

    function formatDate(ms) {
        if (!ms) return "—"
        return Qt.formatDate(new Date(ms), "dd.MM.yyyy")
    }

    header: ToolBar {
        background: Rectangle { color: "#ffffff"; border.color: "#e5e7eb"; border.width: 1 }
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            ToolButton {
                text: "← Назад"
                font.pixelSize: 14
                onClicked: {
                    var sv = page.StackView.view
                    if (sv) {
                        if (sv.depth > 1) sv.pop()
                        else sv.clear()
                    } else { page.back() }
                }
            }
            Item { Layout.fillWidth: true }
            Label {
                text: qsTr("Ведомости на согласование")
                font.bold: true
                font.pixelSize: 16
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                text: "↻"
                font.pixelSize: 18
                onClicked: page.refresh()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // Заголовок с количеством
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Ожидают подтверждения")
                font.pixelSize: 22
                font.bold: true
                color: "#111827"
            }
            Rectangle {
                visible: pendingList.count > 0
                implicitHeight: 28
                implicitWidth: cntLbl.width + 16
                radius: 14
                color: "#fef3c7"
                border.color: "#fcd34d"
                Label {
                    id: cntLbl
                    anchors.centerIn: parent
                    text: String(pendingList.count)
                    font.bold: true
                    font.pixelSize: 13
                    color: "#92400e"
                }
            }
            Item { Layout.fillWidth: true }
        }

        // Список ведомостей
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: "#f3f4f6"
            border.color: "#e5e7eb"
            clip: true

            ListView {
                id: listView
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12
                model: pendingList
                clip: true

                delegate: Rectangle {
                    width: listView.width
                    height: cardCol.implicitHeight + 28
                    radius: 10
                    color: "#ffffff"
                    border.color: "#fcd34d"
                    border.width: 2

                    ColumnLayout {
                        id: cardCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            // Иконка
                            Rectangle {
                                width: 44; height: 44; radius: 10
                                color: "#fef3c7"
                                Label {
                                    anchors.centerIn: parent
                                    text: "📋"
                                    font.pixelSize: 20
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Label {
                                    text: title
                                    font.bold: true
                                    font.pixelSize: 15
                                    color: "#111827"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                RowLayout {
                                    spacing: 16
                                    Label {
                                        text: "👥 " + qsTr("Группа: ") + groupNumber
                                        font.pixelSize: 12
                                        color: "#6b7280"
                                    }
                                    Label {
                                        text: "📚 " + disciplineName
                                        font.pixelSize: 12
                                        color: "#6b7280"
                                    }
                                    Label {
                                        text: "📅 " + formatDate(createdAt)
                                        font.pixelSize: 12
                                        color: "#6b7280"
                                    }
                                }
                            }

                            // Статус-бейдж
                            Rectangle {
                                implicitHeight: 26
                                implicitWidth: statusLbl.width + 20
                                radius: 13
                                color: "#fef3c7"
                                border.color: "#fcd34d"
                                Label {
                                    id: statusLbl
                                    anchors.centerIn: parent
                                    text: qsTr("Ожидает")
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: "#92400e"
                                }
                            }
                        }

                        // Кнопки действий
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            // Просмотр ведомости
                            Button {
                                text: qsTr("👁 Просмотр")
                                onClicked: page.openSheet(sheetId, groupId, groupNumber)
                            }

                            // Подтвердить
                            Button {
                                text: qsTr("✓ Подтвердить")
                                highlighted: true
                                onClicked: {
                                    confirmDialog.sheetId = sheetId
                                    confirmDialog.sheetTitle = title
                                    confirmDialog.open()
                                }
                            }

                            // Отклонить
                            Button {
                                text: qsTr("✕ Отклонить")
                                onClicked: {
                                    rejectDialog.sheetId = sheetId
                                    rejectDialog.sheetTitle = title
                                    rejectDialog.open()
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                // Пусто
                Label {
                    anchors.centerIn: parent
                    visible: pendingList.count === 0
                    text: qsTr("Нет ведомостей на согласование")
                    color: "#9ca3af"
                    font.pixelSize: 15
                }
            }
        }
    }

    // Диалог подтверждения
    Dialog {
        id: confirmDialog
        property string sheetId: ""
        property string sheetTitle: ""
        title: qsTr("Подтвердить ведомость?")
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Cancel | Dialog.Ok

        ColumnLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Подтвердить ведомость:\n«") + confirmDialog.sheetTitle + qsTr("»?\n\nПосле подтверждения статус изменится на «Согласовано».")
                color: "#374151"
            }
        }
        onAccepted: {
            if (appController.approveGradeSheet(confirmDialog.sheetId))
                page.refresh()
        }
    }

    // Диалог отклонения
    Dialog {
        id: rejectDialog
        property string sheetId: ""
        property string sheetTitle: ""
        title: qsTr("Отклонить ведомость?")
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Cancel | Dialog.Ok

        ColumnLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Вернуть ведомость «") + rejectDialog.sheetTitle + qsTr("» на доработку?\n\nСтатус изменится обратно на «Черновик».")
                color: "#374151"
            }
        }
        onAccepted: {
            appController.rejectGradeSheet(rejectDialog.sheetId)
            page.refresh()
        }
    }

    Connections {
        target: appController
        function onPendingSheetsChanged() { page.refresh() }
    }

    Component.onCompleted: refresh()
}

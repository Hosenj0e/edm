import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: page
    required property string groupId
    required property string groupNumber
    property var stackView: null

    signal back()
    signal openDisciplines(string groupId, string groupNumber)

    function navigateBack() {
        var sv = page.StackView.view
        if (sv) {
            if (sv.depth > 1)
                sv.pop()
            else
                sv.clear()
        } else if (page.stackView) {
            if (page.stackView.depth > 1)
                page.stackView.pop()
            else
                page.stackView.clear()
        } else {
            page.back()
        }
    }

    background: Rectangle { color: "#eceef2" }

    ListModel { id: studentsList }

    function refreshStudents() {
        appController.reloadStudents(page.groupId)
        var m = appController.studentsModel
        studentsList.clear()
        if (!m || typeof m.count !== "function")
            return
        for (var i = 0; i < m.count(); ++i) {
            var s = m.itemAt(i)
            if (s && s.studentId)
                studentsList.append(s)
        }
    }

    header: ToolBar {
        background: Rectangle { color: "#ffffff" }
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            ToolButton {
                text: "← Назад"
                font.pixelSize: 14
                z: 1
                onClicked: page.navigateBack()
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label {
                    text: page.groupNumber
                    font.bold: true
                    font.pixelSize: 17
                }
                Label {
                    text: qsTr("Студенты группы")
                    font.pixelSize: 11
                    color: "#6b7280"
                }
            }
            Button {
                text: qsTr("Ведомости")
                highlighted: true
                onClicked: page.openDisciplines(page.groupId, page.groupNumber)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: qsTr("+ Студент")
                onClicked: addStudentDialog.open()
            }
            Button {
                text: qsTr("Импорт списком")
                onClicked: importDialog.open()
            }
            Item { Layout.fillWidth: true }
            Label {
                text: qsTr("Всего: ") + studentsList.count
                color: "#6b7280"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: "#ffffff"
            border.color: "#e5e7eb"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    color: "#f3f4f6"
                    radius: 6
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        Label { text: qsTr("№"); font.bold: true; Layout.preferredWidth: 60 }
                        Label { text: qsTr("ФИО"); font.bold: true; Layout.fillWidth: true }
                        Label { text: qsTr("Статус"); font.bold: true; Layout.preferredWidth: 120 }
                        Item { Layout.preferredWidth: 40 }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: studentsList
                    spacing: 2

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 44
                        color: index % 2 === 0 ? "#ffffff" : "#fafafa"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            Label {
                                text: studentNumber
                                Layout.preferredWidth: 60
                                color: "#374151"
                            }
                            Label {
                                text: fio
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                font.pixelSize: 14
                            }
                            Label {
                                text: status
                                Layout.preferredWidth: 120
                                color: "#6b7280"
                                font.pixelSize: 12
                            }
                            ToolButton {
                                text: "✕"
                                onClicked: {
                                    appController.removeStudent(studentId)
                                    page.refreshStudents()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: addStudentDialog
        title: qsTr("Добавить студента")
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: 400
        anchors.centerIn: parent

        ColumnLayout {
            spacing: 8
            Label { text: qsTr("Номер (зачётный / табельный)") }
            TextField { id: stNum; Layout.fillWidth: true }
            Label { text: qsTr("ФИО") }
            TextField { id: stFio; Layout.fillWidth: true }
            Label { text: qsTr("Статус") }
            ComboBox {
                id: stStatus
                Layout.fillWidth: true
                model: [qsTr("обучается"), qsTr("академ"), qsTr("отчислен"), qsTr("выпуск")]
            }
        }
        onAccepted: {
            appController.addStudent(page.groupId, stNum.text, stFio.text, stStatus.currentText)
            page.refreshStudents()
        }
    }

    Dialog {
        id: importDialog
        title: qsTr("Импорт студентов")
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: 480
        height: 320
        anchors.centerIn: parent

        ColumnLayout {
            anchors.fill: parent
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("По одной записи на строку:\nномер/ФИО/статус\n\nПример:\n101/Иванов Иван Иванович/обучается")
                font.pixelSize: 12
                color: "#6b7280"
            }
            TextArea {
                id: importArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                wrapMode: Text.Wrap
                placeholderText: "101/Иванов И.И./обучается"
            }
        }
        onAccepted: {
            appController.importStudents(page.groupId, importArea.text)
            importArea.text = ""
            page.refreshStudents()
        }
    }

    Component.onCompleted: refreshStudents()
}

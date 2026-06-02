import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: page
    required property string groupId
    required property string groupNumber
    property var stackView: null

    signal back()
    signal openGradeSheet(string sheetId, string groupId, string groupNumber)

    function navigateBack() {
        var sv = page.StackView.view
        if (sv) {
            sv.pop()
        } else if (page.stackView) {
            page.stackView.pop()
        } else {
            page.back()
        }
    }

    background: Rectangle { color: "#eceef2" }

    ListModel { id: disciplinesList }
    ListModel { id: sheetsList }

    function refreshDisciplines() {
        appController.reloadDisciplines()
        var m = appController.disciplinesModel
        disciplinesList.clear()
        if (!m) return
        for (var i = 0; i < m.count(); ++i) {
            var d = m.itemAt(i)
            if (d && d.disciplineId)
                disciplinesList.append(d)
        }
    }

    function refreshSheets() {
        appController.reloadGradeSheets(page.groupId)
        var m = appController.gradeSheetsModel
        sheetsList.clear()
        if (!m) return
        for (var i = 0; i < m.count(); ++i) {
            var s = m.itemAt(i)
            if (s && s.sheetId)
                sheetsList.append(s)
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
                z: 100
                onClicked: {
                    var sv = page.StackView.view
                    if (sv) {
                        if (sv.depth > 1)
                            sv.pop()
                        else
                            sv.clear()
                    } else {
                        page.back()
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("Дисциплины — ") + page.groupNumber
                    font.bold: true
                    font.pixelSize: 16
                }
                Label {
                    text: qsTr("Выберите дисциплину и создайте ведомость")
                    font.pixelSize: 11
                    color: "#6b7280"
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
            Label { text: qsTr("Дисциплины"); font.bold: true; font.pixelSize: 14 }

            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: newDiscField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Новая дисциплина")
                }
                Button {
                    text: qsTr("Добавить")
                    onClicked: {
                        if (appController.addDiscipline(newDiscField.text))
                            newDiscField.text = ""
                        refreshDisciplines()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 10
                color: "#ffffff"
                border.color: "#e5e7eb"
                ListView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: disciplinesList
                    spacing: 6
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 48
                        radius: 8
                        color: "#f9fafb"
                        border.color: "#e5e7eb"
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            Label {
                                text: name
                                Layout.fillWidth: true
                                font.pixelSize: 14
                            }
                            Button {
                                text: qsTr("Создать ведомость")
                                highlighted: true
                                onClicked: {
                                    createSheetDialog.selectedDisciplineId = disciplineId
                                    createSheetDialog.selectedDisciplineName = name
                                    createSheetDialog.open()
                                }
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
            Label { text: qsTr("Ведомости группы"); font.bold: true; font.pixelSize: 14 }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 10
                color: "#ffffff"
                border.color: "#e5e7eb"
                ListView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: sheetsList
                    spacing: 6
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 56
                        radius: 8
                        color: "#f9fafb"
                        border.color: "#e5e7eb"
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    text: title
                                    font.bold: true
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: disciplineName
                                    font.pixelSize: 11
                                    color: "#6b7280"
                                }
                            }
                            Button {
                                text: qsTr("Открыть")
                                onClicked: page.openGradeSheet(sheetId, page.groupId, page.groupNumber)
                            }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: sheetsList.count === 0
                    text: qsTr("Ведомостей пока нет")
                    color: "#9ca3af"
                }
            }
        }
    }

    // Диалог создания ведомости с выбором типа
    Dialog {
        id: createSheetDialog
        property string selectedDisciplineId: ""
        property string selectedDisciplineName: ""
        
        title: qsTr("Создать ведомость")
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        anchors.centerIn: parent
        width: 420

        ColumnLayout {
            spacing: 12
            
            Label {
                text: qsTr("Дисциплина: ") + createSheetDialog.selectedDisciplineName
                font.bold: true
                font.pixelSize: 13
                Layout.fillWidth: true
            }
            
            Label {
                text: qsTr("Группа: ") + page.groupNumber
                font.pixelSize: 12
                color: "#6b7280"
            }
            
            Item { Layout.preferredHeight: 8 }
            
            Label {
                text: qsTr("Тип аттестации:")
                font.pixelSize: 12
                font.bold: true
            }
            
            ButtonGroup {
                id: examTypeGroup
            }
            
            ColumnLayout {
                spacing: 8
                Layout.fillWidth: true
                
                RadioButton {
                    id: examRadio
                    text: qsTr("Экзамен")
                    checked: true
                    ButtonGroup.group: examTypeGroup
                    font.pixelSize: 13
                }
                
                RadioButton {
                    id: creditRadio
                    text: qsTr("Зачет")
                    ButtonGroup.group: examTypeGroup
                    font.pixelSize: 13
                }
            }
        }
        
        onAccepted: {
            var examType = examRadio.checked ? "exam" : "credit"
            var sid = appController.createGradeSheet(page.groupId, createSheetDialog.selectedDisciplineId, examType)
            if (sid.length > 0) {
                refreshSheets()
                page.openGradeSheet(sid, page.groupId, page.groupNumber)
            }
        }
    }

    Component.onCompleted: {
        refreshDisciplines()
        refreshSheets()
    }
}

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: page
    required property string sheetId
    required property string groupId
    required property string groupNumber
    property var stackView: null

    signal back()

    background: Rectangle { color: "#ffffff" }

    property var sheetMeta: ({})
    ListModel { id: studentsList }

    function refresh() {
        page.sheetMeta = appController.gradeSheetInfo(page.sheetId)
        appController.reloadStudents(page.groupId)
        var m = appController.studentsModel
        studentsList.clear()
        if (!m) return
        for (var i = 0; i < m.count(); ++i) {
            var s = m.itemAt(i)
            if (s && s.studentId)
                studentsList.append(s)
        }
    }

    function formatDate(ms) {
        if (!ms) return ""
        return Qt.formatDate(new Date(ms), "dd.MM.yyyy")
    }

    header: ToolBar {
        background: Rectangle { color: "#ffffff"; border.color: "#e5e7eb"; border.width: 1 }
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            Button {
                text: qsTr("← Назад")
                flat: true
                font.pixelSize: 14
                onClicked: {
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
            }
            Item { Layout.fillWidth: true }
            Label {
                text: qsTr("Аттестационная ведомость")
                font.bold: true
                font.pixelSize: 16
            }
            Item { Layout.fillWidth: true }

            // Кнопка скачать (только admin и deputy)
            Button {
                text: qsTr("⬇ Скачать")
                visible: appController.currentUserRole === "admin" || appController.currentUserRole === "deputy"
                onClicked: saveDialog.open()
            }

            // Кнопка печать
            Button {
                text: qsTr("🖨 Печать")
                visible: appController.currentUserRole === "admin" || appController.currentUserRole === "deputy"
                onClicked: printDialog.open()
            }

            // Кнопка добавить студента (только admin и deputy)
            Button {
                text: qsTr("+ Студент")
                highlighted: true
                visible: appController.currentUserRole === "admin" || appController.currentUserRole === "deputy"
                onClicked: addStudentDialog.open()
            }

            // Кнопка отправить на согласование (только teacher)
            Button {
                text: qsTr("📤 На согласование")
                highlighted: true
                visible: appController.currentUserRole === "teacher"
                onClicked: submitDialog.open()
            }
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 24
        contentHeight: sheetContent.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: sheetContent
            width: parent.width
            spacing: 0

            // === Шапка ведомости ===
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: headerCol.implicitHeight + 32
                color: "#ffffff"
                border.color: "#000000"
                border.width: 1

                ColumnLayout {
                    id: headerCol
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 6

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Министерство науки и высшего образования Российской Федерации")
                        font.pixelSize: 10
                        color: "#374151"
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Федеральное государственное бюджетное образовательное учреждение высшего образования")
                        font.pixelSize: 10
                        color: "#374151"
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("ПОЛИТЕХНИЧЕСКИЙ ИНСТИТУТ\nПОЛИТЕХНИЧЕСКИЙ КОЛЛЕДЖ")
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Item { Layout.preferredHeight: 12 }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Аттестационная ведомость №_____")
                            font.pixelSize: 13
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: qsTr("Дата ") + formatDate(page.sheetMeta.createdAt || Date.now())
                            font.pixelSize: 12
                        }
                        Label {
                            text: qsTr("  Семестр ___")
                            font.pixelSize: 12
                        }
                    }

                    Label {
                        text: {
                            var examType = page.sheetMeta.examType || "exam"
                            var typeText = examType === "credit" ? qsTr("Зачет") : qsTr("Экзамен")
                            return qsTr("Вид промежуточной аттестации: ") + typeText
                        }
                        font.pixelSize: 11
                        font.bold: true
                        color: "#1f2937"
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Дисциплина: "); font.pixelSize: 12 }
                        Label {
                            text: page.sheetMeta.disciplineName || "_______________"
                            font.pixelSize: 12
                            font.bold: true
                            font.underline: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Специальность: 09.02.07 Информационные системы и программирование"); font.pixelSize: 11 }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Курс ___ "); font.pixelSize: 12 }
                        Label {
                            text: qsTr("Группа ") + page.groupNumber
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                }
            }

            // === Таблица студентов ===
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: tableCol.implicitHeight
                color: "#ffffff"
                border.color: "#000000"
                border.width: 1

                ColumnLayout {
                    id: tableCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 0

                    // Заголовок таблицы
                    Rectangle {
                        Layout.fillWidth: true
                        height: 50
                        color: "#f9fafb"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.preferredWidth: 40
                                Layout.fillHeight: true
                                border.color: "#000000"
                                border.width: 1
                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("№\nп/п")
                                    font.pixelSize: 10
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                border.color: "#000000"
                                border.width: 1
                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("Фамилия, И.О. студента")
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                            Rectangle {
                                Layout.preferredWidth: 80
                                Layout.fillHeight: true
                                border.color: "#000000"
                                border.width: 1
                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("Отметка\nо допуске")
                                    font.pixelSize: 9
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                            Rectangle {
                                Layout.preferredWidth: 70
                                Layout.fillHeight: true
                                border.color: "#000000"
                                border.width: 1
                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("Оценка")
                                    font.pixelSize: 10
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                            Rectangle {
                                Layout.preferredWidth: 90
                                Layout.fillHeight: true
                                border.color: "#000000"
                                border.width: 1
                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("Подпись\nпрепод.")
                                    font.pixelSize: 9
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                            Rectangle {
                                Layout.preferredWidth: 70
                                Layout.fillHeight: true
                                border.color: "#000000"
                                border.width: 1
                                Label {
                                    anchors.centerIn: parent
                                    text: qsTr("Действия")
                                    font.pixelSize: 9
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                        }
                    }

                    // Строки студентов
                    Repeater {
                        model: studentsList
                        delegate: Rectangle {
                            id: rowRect
                            Layout.fillWidth: true
                            height: rowRect.editing ? 48 : 36
                            color: index % 2 === 0 ? "#ffffff" : "#fafafa"

                            // Локальные свойства для редактирования
                            property bool editing: false

                            RowLayout {
                                anchors.fill: parent
                                spacing: 0

                                // № п/п
                                Rectangle {
                                    Layout.preferredWidth: 40
                                    Layout.fillHeight: true
                                    border.color: "#d1d5db"
                                    border.width: 1
                                    Label {
                                        anchors.centerIn: parent
                                        text: String(index + 1)
                                        font.pixelSize: 11
                                    }
                                }
                                // ФИО — при редактировании TextField, иначе Label
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    border.color: rowRect.editing ? "#6366f1" : "#d1d5db"
                                    border.width: rowRect.editing ? 2 : 1
                                    color: rowRect.editing ? "#f5f3ff" : "transparent"

                                    Label {
                                        visible: !rowRect.editing
                                        anchors.left: parent.left
                                        anchors.leftMargin: 6
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: fio
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        width: parent.width - 12
                                    }
                                    TextField {
                                        id: fioEdit
                                        visible: rowRect.editing
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        text: fio
                                        font.pixelSize: 11
                                    }
                                }
                                // Статус
                                Rectangle {
                                    Layout.preferredWidth: 80
                                    Layout.fillHeight: true
                                    border.color: rowRect.editing ? "#6366f1" : "#d1d5db"
                                    border.width: rowRect.editing ? 2 : 1
                                    color: rowRect.editing ? "#f5f3ff" : "transparent"

                                    Label {
                                        visible: !rowRect.editing
                                        anchors.centerIn: parent
                                        text: status === "обучается" ? "доп." : status
                                        font.pixelSize: 10
                                        color: "#6b7280"
                                    }
                                    ComboBox {
                                        id: statusEdit
                                        visible: rowRect.editing
                                        anchors.fill: parent
                                        model: ["обучается", "academ", "отчислен"]
                                        Component.onCompleted: {
                                            var idx = model.indexOf(status)
                                            currentIndex = idx >= 0 ? idx : 0
                                        }
                                        font.pixelSize: 10
                                    }
                                }
                                // Оценка — всегда редактируемая с автосохранением
                                Rectangle {
                                    Layout.preferredWidth: 70
                                    Layout.fillHeight: true
                                    border.color: gradeField.activeFocus ? "#6366f1" : "#d1d5db"
                                    border.width: gradeField.activeFocus ? 2 : 1
                                    color: gradeField.activeFocus ? "#f0fdf4" : "transparent"

                                    TextField {
                                        id: gradeField
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        horizontalAlignment: Text.AlignHCenter
                                        font.pixelSize: 13
                                        font.bold: text.length > 0
                                        text: grade || ""
                                        placeholderText: "—"
                                        validator: RegularExpressionValidator {
                                            regularExpression: /^[1-5]?$/
                                        }
                                        
                                        // Автосохранение при изменении текста
                                        onTextChanged: {
                                            if (text !== (grade || "")) {
                                                saveTimer.restart()
                                            }
                                        }
                                        
                                        // Также сохраняем при потере фокуса
                                        onEditingFinished: {
                                            saveTimer.stop()
                                            appController.updateStudent(studentId, fio, status, gradeField.text)
                                            studentsList.setProperty(index, "grade", gradeField.text)
                                        }
                                        
                                        // Таймер для отложенного автосохранения (300ms после последнего изменения)
                                        Timer {
                                            id: saveTimer
                                            interval: 300
                                            repeat: false
                                            onTriggered: {
                                                appController.updateStudent(studentId, fio, status, gradeField.text)
                                                studentsList.setProperty(index, "grade", gradeField.text)
                                            }
                                        }
                                    }
                                }
                                // Подпись
                                Rectangle {
                                    Layout.preferredWidth: 90
                                    Layout.fillHeight: true
                                    border.color: "#d1d5db"
                                    border.width: 1
                                }
                                // Действия: редактировать / сохранить / удалить
                                Rectangle {
                                    Layout.preferredWidth: 70
                                    Layout.fillHeight: true
                                    border.color: "#d1d5db"
                                    border.width: 1
                                    color: "transparent"

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 4

                                        // Кнопка редактировать / сохранить
                                        Label {
                                            text: rowRect.editing ? "💾" : "✎"
                                            font.pixelSize: 14
                                            color: rowRect.editing ? "#16a34a" : "#2563eb"
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (rowRect.editing) {
                                                        // Сохраняем
                                                        appController.updateStudent(
                                                            studentId,
                                                            fioEdit.text,
                                                            statusEdit.currentText,
                                                            gradeField.text
                                                        )
                                                        // Обновляем модель
                                                        studentsList.setProperty(index, "fio", fioEdit.text)
                                                        studentsList.setProperty(index, "status", statusEdit.currentText)
                                                        studentsList.setProperty(index, "grade", gradeField.text)
                                                        rowRect.editing = false
                                                    } else {
                                                        rowRect.editing = true
                                                    }
                                                }
                                            }
                                        }

                                        // Кнопка удалить
                                        Label {
                                            text: "✕"
                                            font.pixelSize: 14
                                            color: "#dc2626"
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    confirmDeleteDialog.studentId = studentId
                                                    confirmDeleteDialog.studentFio = fio
                                                    confirmDeleteDialog.open()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Пустая строка если нет студентов
                    Rectangle {
                        visible: studentsList.count === 0
                        Layout.fillWidth: true
                        height: 60
                        color: "#f9fafb"
                        Label {
                            anchors.centerIn: parent
                            text: qsTr("Нет студентов. Добавьте через кнопку выше.")
                            color: "#9ca3af"
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }

            // === Подвал ведомости ===
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: footerCol.implicitHeight + 24
                color: "#ffffff"
                border.color: "#000000"
                border.width: 1

                ColumnLayout {
                    id: footerCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        text: qsTr("* Оценки проставляются цифрами и в скобках прописью")
                        font.pixelSize: 10
                        color: "#6b7280"
                    }

                    Item { Layout.preferredHeight: 8 }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("\"___\" ___________ 20__ г."); font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        ColumnLayout {
                            spacing: 4
                            Label { text: qsTr("Итого оценок: 5 ___"); font.pixelSize: 11 }
                            Label { text: qsTr("                        4 ___"); font.pixelSize: 11 }
                            Label { text: qsTr("                        3 ___"); font.pixelSize: 11 }
                            Label { text: qsTr("                        2 ___"); font.pixelSize: 11 }
                        }
                    }

                    Item { Layout.preferredHeight: 12 }

                    Label { text: qsTr("Подпись преподавателя ___________________"); font.pixelSize: 11 }
                    Label { text: qsTr("Зам. директора по УМ и ВР/ зав.уч.частью /зав отделением ___________________"); font.pixelSize: 11 }

                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        Label { text: qsTr("Не аттестовано ___"); font.pixelSize: 11 }
                    }
                }
            }
        }
    }

    // === Диалог подтверждения удаления ===
    Dialog {
        id: confirmDeleteDialog
        property string studentId: ""
        property string studentFio: ""
        title: qsTr("Удалить студента из ведомости?")
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        anchors.centerIn: parent
        Label {
            text: qsTr("Удалить: ") + confirmDeleteDialog.studentFio + "?"
            wrapMode: Text.WordWrap
        }
        onAccepted: {
            appController.removeStudent(confirmDeleteDialog.studentId)
            page.refresh()
        }
    }

    // === Диалог добавления студента ===
    Dialog {
        id: addStudentDialog
        title: qsTr("Добавить студента в ведомость")
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        anchors.centerIn: parent
        width: 400

        ColumnLayout {
            spacing: 8
            Label { text: qsTr("Номер (зачётный / табельный)") }
            TextField { id: newStNum; Layout.fillWidth: true; placeholderText: "1" }
            Label { text: qsTr("ФИО") }
            TextField { id: newStFio; Layout.fillWidth: true; placeholderText: "Иванов Иван Иванович" }
            Label { text: qsTr("Статус") }
            ComboBox {
                id: newStStatus
                Layout.fillWidth: true
                model: [qsTr("обучается"), qsTr("академ"), qsTr("отчислен")]
            }
        }
        onAccepted: {
            appController.addStudent(page.groupId, newStNum.text, newStFio.text, newStStatus.currentText)
            newStNum.text = ""
            newStFio.text = ""
            page.refresh()
        }
    }

    // === Диалог скачивания ===
    Dialog {
        id: saveDialog
        title: qsTr("Скачать ведомость")
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Cancel | Dialog.Ok

        ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Ведомость будет сохранена в формате HTML на рабочий стол.")
                color: "#374151"
            }
            TextField {
                id: saveFileName
                Layout.fillWidth: true
                text: "vedomost_" + page.groupNumber + "_" + (page.sheetMeta.disciplineName || "disc")
                placeholderText: qsTr("Имя файла (без расширения)")
            }
        }
        onAccepted: {
            var fileName = saveFileName.text.trim()
            if (fileName.length === 0) fileName = "vedomost"
            var path = appController.exportGradeSheetHtml(page.sheetId, fileName)
            if (path.length > 0)
                saveSuccessMsg.text = qsTr("Сохранено: ") + path
            else
                saveSuccessMsg.text = qsTr("Ошибка при сохранении файла.")
            saveSuccessDialog.open()
        }
    }

    Dialog {
        id: saveSuccessDialog
        title: qsTr("Готово")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok
        Label {
            id: saveSuccessMsg
            wrapMode: Text.WordWrap
            width: 360
        }
    }

    // === Диалог печати ===
    Dialog {
        id: printDialog
        title: qsTr("Печать ведомости")
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Cancel | Dialog.Ok

        ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Ведомость будет сохранена как HTML-файл и открыта в браузере для печати.")
                color: "#374151"
            }
        }
        onAccepted: {
            var path = appController.exportGradeSheetHtml(page.sheetId, "print_temp")
            if (path.length > 0)
                Qt.openUrlExternally("file:///" + path)
        }
    }

    // === Диалог отправки на согласование (для преподавателя) ===
    Dialog {
        id: submitDialog
        title: qsTr("Отправить на согласование")
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Cancel | Dialog.Ok

        ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Ведомость будет отправлена заместителю директора на согласование.\n\nПосле отправки редактирование будет недоступно.")
                color: "#374151"
            }
        }
        onAccepted: {
            appController.submitGradeSheetForApproval(page.sheetId)
            page.refresh()
        }
    }

    Component.onCompleted: refresh()
}

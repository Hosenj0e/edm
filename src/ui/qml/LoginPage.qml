import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: page
    background: Rectangle {
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: "#e8f0fe" }
            GradientStop { position: 1.0; color: "#f0f4ff" }
        }
    }

    ColumnLayout {
        width: Math.min(420, parent.width - 48)
        anchors.centerIn: parent
        spacing: 0

        // Логотип
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 72; height: 72; radius: 18
                color: "#1a1a1a"
                Text { anchors.centerIn: parent; text: "🗒"; font.pixelSize: 32 }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("ДокументПро")
                font.pixelSize: 26; font.bold: true; color: "#111827"
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Электронный документооборот")
                font.pixelSize: 13; color: "#6b7280"
            }
        }

        Item { implicitHeight: 28 }

        // Карточка формы
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: cardCol.implicitHeight + 36
            radius: 16; color: "#ffffff"
            border.color: "#e5e7eb"; border.width: 1

            ColumnLayout {
                id: cardCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 28 }
                spacing: 0

                Text { text: qsTr("Вход в систему"); font.pixelSize: 22; font.bold: true; color: "#111827" }
                Item { implicitHeight: 6 }
                Text { text: qsTr("Введите свои учетные данные для доступа"); font.pixelSize: 13; color: "#6b7280" }
                Item { implicitHeight: 24 }

                // Поле логина
                Text { text: qsTr("Имя пользователя"); font.pixelSize: 13; font.bold: true; color: "#374151" }
                Item { implicitHeight: 8 }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 50; radius: 10
                    color: "#f9fafb"
                    border.color: loginField.activeFocus ? "#6366f1" : "#e5e7eb"
                    border.width: loginField.activeFocus ? 2 : 1

                    Row {
                        anchors { fill: parent; leftMargin: 14; rightMargin: 14 }
                        spacing: 10

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "👤"; font.pixelSize: 16; color: "#9ca3af"
                        }

                        Item {
                            width: parent.width - 14 - 10 - 14
                            height: parent.height

                            TextInput {
                                id: loginField
                                anchors { fill: parent; topMargin: 2; bottomMargin: 2 }
                                verticalAlignment: TextInput.AlignVCenter
                                font.pixelSize: 14; color: "#111827"
                                Keys.onReturnPressed: passField.forceActiveFocus()

                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: qsTr("Введите имя пользователя")
                                    font.pixelSize: 14; color: "#9ca3af"
                                    visible: loginField.text.length === 0 && !loginField.activeFocus
                                }
                            }
                        }
                    }
                }

                Item { implicitHeight: 16 }

                // Поле пароля
                Text { text: qsTr("Пароль"); font.pixelSize: 13; font.bold: true; color: "#374151" }
                Item { implicitHeight: 8 }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 50; radius: 10
                    color: "#f9fafb"
                    border.color: passField.activeFocus ? "#6366f1" : "#e5e7eb"
                    border.width: passField.activeFocus ? 2 : 1

                    Row {
                        anchors { fill: parent; leftMargin: 14; rightMargin: 14 }
                        spacing: 10

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "🔒"; font.pixelSize: 16; color: "#9ca3af"
                        }

                        Item {
                            width: parent.width - 14 - 10 - 36 - 10 - 14
                            height: parent.height

                            TextInput {
                                id: passField
                                anchors { fill: parent; topMargin: 2; bottomMargin: 2 }
                                verticalAlignment: TextInput.AlignVCenter
                                echoMode: showPassBtn.showPassword ? TextInput.Normal : TextInput.Password
                                font.pixelSize: 14; color: "#111827"
                                Keys.onReturnPressed: doLogin()

                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: qsTr("Введите пароль")
                                    font.pixelSize: 14; color: "#9ca3af"
                                    visible: passField.text.length === 0 && !passField.activeFocus
                                }
                            }
                        }

                        Rectangle {
                            id: showPassBtn
                            property bool showPassword: false
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36; height: 36; radius: 8
                            color: showPassMa.containsMouse ? "#f3f4f6" : "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: showPassBtn.showPassword ? "🙈" : "👁"
                                font.pixelSize: 16; color: "#9ca3af"
                            }
                            MouseArea {
                                id: showPassMa
                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: showPassBtn.showPassword = !showPassBtn.showPassword
                            }
                        }
                    }
                }

                Item { implicitHeight: 16 }

                // Запомнить / Забыли пароль
                Row {
                    Layout.fillWidth: true
                    spacing: 0

                    Row {
                        id: rememberRow
                        spacing: 8

                        Rectangle {
                            id: rememberBox
                            property bool checked: false
                            width: 18; height: 18; radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: rememberBox.checked ? "#1a1a1a" : "#ffffff"
                            border.color: rememberBox.checked ? "#1a1a1a" : "#9ca3af"
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                visible: rememberBox.checked
                                text: "✓"; font.pixelSize: 11; font.bold: true; color: "#ffffff"
                            }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: rememberBox.checked = !rememberBox.checked
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Запомнить меня"); font.pixelSize: 13; color: "#374151"
                        }
                    }

                    Item { Layout.fillWidth: true; width: 1; height: 1 }

                    Text {
                        id: forgotText
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Забыли пароль?"); font.pixelSize: 13; font.bold: true; color: "#374151"
                    }
                }

                // Ошибка
                Item { implicitHeight: errRect.visible ? 12 : 0 }
                Rectangle {
                    id: errRect
                    Layout.fillWidth: true
                    visible: errText.text.length > 0
                    implicitHeight: errText.implicitHeight + 16
                    radius: 8; color: "#fef2f2"
                    border.color: "#fecaca"; border.width: 1
                    Text {
                        id: errText
                        anchors { fill: parent; margins: 10 }
                        wrapMode: Text.WordWrap; color: "#dc2626"; font.pixelSize: 12
                    }
                }

                Item { implicitHeight: 20 }

                // Кнопка Войти
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 50; radius: 10
                    color: loginBtnMa.pressed ? "#333333" : (loginBtnMa.containsMouse ? "#2d2d2d" : "#1a1a1a")
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Войти")
                        font.pixelSize: 15; font.bold: true; color: "#ffffff"
                        font.letterSpacing: 0.5
                    }
                    MouseArea {
                        id: loginBtnMa
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: doLogin()
                    }
                }

                Item { implicitHeight: 16 }

                // Защищённое соединение
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: secureRow.implicitHeight + 20
                    radius: 10; color: "#eff6ff"
                    border.color: "#bfdbfe"; border.width: 1

                    Row {
                        id: secureRow
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 14 }
                        spacing: 12

                        Text { text: "🛡"; font.pixelSize: 20; color: "#3b82f6"; anchors.verticalCenter: parent.verticalCenter }

                        Column {
                            spacing: 3; anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 20 - 12 - 14
                            Text {
                                text: qsTr("Защищенное соединение")
                                font.pixelSize: 13; font.bold: true; color: "#1d4ed8"
                            }
                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                text: qsTr("Ваши данные защищены с помощью локального шифрования. Работает в офлайн-режиме.")
                                font.pixelSize: 11; color: "#3b82f6"
                            }
                        }
                    }
                }

                Item { implicitHeight: 20 }

                Row {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 4
                    Text { text: qsTr("Нет аккаунта?"); font.pixelSize: 13; color: "#6b7280" }
                    Text { text: qsTr("Обратитесь к администратору"); font.pixelSize: 13; font.bold: true; color: "#374151" }
                }

                Item { implicitHeight: 4 }
            }
        }
    }

    function doLogin() {
        errText.text = ""
        if (!authController.login(loginField.text, passField.text)) {
            errText.text = authController.lastError
            return
        }
        appController.openSession(authController.activeUserDataPath(),
                                  authController.currentLogin,
                                  authController.currentDisplayName,
                                  authController.currentRole)
    }
}

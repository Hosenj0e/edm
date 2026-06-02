import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: "#eceef2"
        z: -2
    }

    property string selectedGroupId: ""
    property string navFilter: "all" // all | mine | withSheets | empty
    property string searchText: ""
    property int pendingCount: 0

    function refreshPendingCount() {
        var arr = appController.pendingSheetsList()
        pendingCount = arr ? arr.length : 0
    }

    ListModel { id: groupList }

    function formatDate(ms) {
        if (!ms) return "—"
        return Qt.formatDate(new Date(ms), "dd.MM.yyyy")
    }

    function countAllGroups() {
        var m = appController.groupsModel
        return (m && typeof m.count === "function") ? m.count() : 0
    }

    function countGroupsWithSheets() {
        var m = appController.groupsModel
        if (!m || typeof m.count !== "function") return 0
        var n = 0
        for (var i = 0; i < m.count(); ++i) {
            var g = m.itemAt(i)
            if (g && g.sheetCount > 0) ++n
        }
        return n
    }

    function countEmptyGroups() {
        var m = appController.groupsModel
        if (!m || typeof m.count !== "function") return 0
        var n = 0
        for (var i = 0; i < m.count(); ++i) {
            var g = m.itemAt(i)
            if (g && g.studentCount === 0) ++n
        }
        return n
    }

    function matchesFilters(g) {
        if (!g || !g.groupId) return false
        if (root.navFilter === "withSheets" && g.sheetCount <= 0) return false
        if (root.navFilter === "empty" && g.studentCount > 0) return false
        if (root.searchText.length > 0) {
            var q = root.searchText.toLowerCase()
            if (String(g.number).toLowerCase().indexOf(q) < 0
                && String(g.subtitle || "").toLowerCase().indexOf(q) < 0
                && String(g.specialty || "").toLowerCase().indexOf(q) < 0)
                return false
        }
        return true
    }

    function syncGroupList() {
        var m = appController.groupsModel
        groupList.clear()
        if (!m || typeof m.count !== "function" || typeof m.itemAt !== "function")
            return
        for (var i = 0; i < m.count(); ++i) {
            var g = m.itemAt(i)
            if (!matchesFilters(g)) continue
            groupList.append(g)
        }
        refreshSelection()
    }

    function refreshSelection() {
        if (selectedGroupId === "") return
        for (var i = 0; i < groupList.count; ++i) {
            if (groupList.get(i).groupId === selectedGroupId) return
        }
        selectedGroupId = ""
    }

    function pushDetail(component, props) {
        var p = props || {}
        p.stackView = detailStack
        detailStack.push(component, p)
        detailStack.forceActiveFocus()
    }

    function openGroupStudents(groupId, groupNumber) {
        pushDetail(groupPageCmp, { "groupId": groupId, "groupNumber": groupNumber })
    }

    function openDisciplines(groupId, groupNumber) {
        pushDetail(disciplinesCmp, { "groupId": groupId, "groupNumber": groupNumber })
    }

    function openGradeSheet(sheetId, groupId, groupNumber) {
        pushDetail(gradeSheetCmp, {
            "sheetId": sheetId,
            "groupId": groupId,
            "groupNumber": groupNumber
        })
    }

    Component.onCompleted: {
        syncGroupList()
        refreshPendingCount()
    }

    Connections {
        target: appController
        function onGroupsChanged() { syncGroupList() }
        function onSessionOpenChanged() {
            if (appController.sessionOpen) {
                syncGroupList()
                refreshPendingCount()
            }
        }
        function onPendingSheetsChanged() { refreshPendingCount() }
        function onPortalSyncFinished() { syncGroupList() }
    }

    component SidebarNavButton: Rectangle {
        property string label: ""
        property string badge: ""
        property bool selected: false
        property bool accentBadge: false
        signal triggered

        implicitHeight: 42
        radius: 8
        color: selected ? "#f3f4f6" : "transparent"
        border.width: selected ? 1 : 0
        border.color: "#e5e7eb"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8
            Text {
                text: label
                font.pixelSize: 14
                color: selected ? "#111827" : "#374151"
                Layout.fillWidth: true
            }
            Rectangle {
                visible: badge !== ""
                implicitHeight: 22
                implicitWidth: Math.max(22, badgeText.width + 12)
                radius: 11
                color: accentBadge ? "#dbeafe" : "#f3f4f6"
                border.width: accentBadge ? 1 : 0
                border.color: "#93c5fd"
                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: badge
                    font.pixelSize: 11
                    font.bold: true
                    color: accentBadge ? "#1d4ed8" : "#4b5563"
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.triggered()
        }
    }

    RowLayout {
        id: mainChrome
        anchors.fill: parent
        spacing: 0
        enabled: detailStack.depth === 0

        Rectangle {
            Layout.preferredWidth: 278
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#e3e5e8"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        width: 40; height: 40; radius: 10; color: "#1a1a1a"
                        Text { anchors.centerIn: parent; text: "DP"; color: "#fff"; font.bold: true; font.pixelSize: 13 }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: qsTr("ДокументПро"); font.pixelSize: 18; font.bold: true; color: "#1a1a1a" }
                        Text { text: qsTr("Учебное заведение"); font.pixelSize: 11; color: "#6b7280" }
                    }
                }

                Item { Layout.preferredHeight: 16 }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: offlineCol.implicitHeight + 20
                    radius: 10
                    color: appController.isOnline ? "#f0fdf4" : "#fff7ed"
                    border.color: appController.isOnline ? "#86efac" : "#fdba74"
                    border.width: 1
                    ColumnLayout {
                        id: offlineCol
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "●"; color: appController.isOnline ? "#16a34a" : "#ea580c"; font.pixelSize: 10 }
                            Text {
                                text: appController.isOnline ? qsTr("Online") : qsTr("Offline")
                                font.bold: true
                                font.pixelSize: 13
                                color: appController.isOnline ? "#15803d" : "#c2410c"
                                Layout.fillWidth: true
                            }
                            Text {
                                text: "↻"
                                color: appController.isOnline ? "#16a34a" : "#ea580c"
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: appController.flushSync()
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            color: appController.isOnline ? "#166534" : "#9a3412"
                            text: appController.isOnline
                                  ? qsTr("Подключено. Данные синхронизируются с сервером")
                                  : qsTr("Работа в офлайн-режиме. Данные защищены локальным шифрованием")
                        }
                    }
                }

                Item { Layout.preferredHeight: 16 }

                Text { text: qsTr("Меню"); font.pixelSize: 11; font.bold: true; color: "#9ca3af" }

                SidebarNavButton {
                    Layout.fillWidth: true
                    label: qsTr("Все группы")
                    badge: String(countAllGroups())
                    selected: root.navFilter === "all"
                    onTriggered: { root.navFilter = "all"; syncGroupList() }
                }
                SidebarNavButton {
                    Layout.fillWidth: true
                    label: qsTr("Мои группы")
                    badge: ""
                    selected: root.navFilter === "mine"
                    onTriggered: { root.navFilter = "mine"; syncGroupList() }
                }
                SidebarNavButton {
                    Layout.fillWidth: true
                    label: qsTr("С ведомостями")
                    badge: String(countGroupsWithSheets())
                    accentBadge: true
                    selected: root.navFilter === "withSheets"
                    onTriggered: { root.navFilter = "withSheets"; syncGroupList() }
                }
                SidebarNavButton {
                    Layout.fillWidth: true
                    label: qsTr("Без студентов")
                    badge: String(countEmptyGroups())
                    selected: root.navFilter === "empty"
                    onTriggered: { root.navFilter = "empty"; syncGroupList() }
                }

                Item { Layout.preferredHeight: 4 }

                // Вкладка "На согласование" — только для deputy и admin
                SidebarNavButton {
                    Layout.fillWidth: true
                    label: qsTr("На согласование")
                    badge: String(pendingCount)
                    accentBadge: pendingCount > 0
                    selected: false
                    visible: appController.currentUserRole === "admin" || appController.currentUserRole === "deputy"
                    onTriggered: root.pushDetail(approvalPageCmp, {})
                }

                Item { Layout.preferredHeight: 8 }

                Rectangle {
                    Layout.fillWidth: true
                    visible: appController.currentUserRole === "admin"
                    implicitHeight: 42
                    radius: 8
                    color: manageAccMa.containsMouse ? "#f3f4f6" : "transparent"
                    border.width: manageAccMa.containsMouse ? 1 : 0
                    border.color: "#e5e7eb"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Text { text: "⚙"; font.pixelSize: 14 }
                        Text { text: qsTr("Управление аккаунтами"); Layout.fillWidth: true; font.pixelSize: 14 }
                    }
                    MouseArea {
                        id: manageAccMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: manageAccountsDialog.open()
                    }
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        width: 40; height: 40; radius: 20; color: "#e5e7eb"
                        Text { anchors.centerIn: parent; text: "👤"; font.pixelSize: 18 }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: appController.currentUserDisplayName; font.bold: true; font.pixelSize: 14 }
                        Text { text: appController.currentUserLogin; font.pixelSize: 11; color: "#6b7280" }
                        // Бейдж роли
                        Rectangle {
                            implicitHeight: 18
                            implicitWidth: roleLabel.width + 12
                            radius: 9
                            color: {
                                var r = appController.currentUserRole
                                if (r === "admin") return "#fef3c7"
                                if (r === "deputy") return "#ede9fe"
                                return "#e0f2fe"
                            }
                            Text {
                                id: roleLabel
                                anchors.centerIn: parent
                                font.pixelSize: 10
                                font.bold: true
                                color: {
                                    var r = appController.currentUserRole
                                    if (r === "admin") return "#92400e"
                                    if (r === "deputy") return "#5b21b6"
                                    return "#0369a1"
                                }
                                text: {
                                    var r = appController.currentUserRole
                                    if (r === "admin") return qsTr("Администратор")
                                    if (r === "deputy") return qsTr("Зам. директора")
                                    return qsTr("Преподаватель")
                                }
                            }
                        }
                        Text {
                            text: qsTr("Выйти")
                            font.pixelSize: 12
                            color: "#2563eb"
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    detailStack.clear()
                                    appController.closeSession()
                                    authController.logout()
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#eceef2"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Учебные группы")
                        font.pixelSize: 28
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        implicitHeight: 44
                        implicitWidth: portalLbl.width + 36
                        radius: 8
                        color: portalMa.pressed ? "#dbeafe" : "#eff6ff"
                        border.color: "#93c5fd"
                        border.width: 1
                        visible: !appController.portalSyncBusy
                        opacity: appController.sessionOpen ? 1 : 0.4
                        Text {
                            id: portalLbl
                            anchors.centerIn: parent
                            text: qsTr("⟳ Импорт групп")
                            color: "#1d4ed8"
                            font.bold: true
                            font.pixelSize: 13
                        }
                        MouseArea {
                            id: portalMa
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            enabled: appController.sessionOpen
                            onClicked: portalSyncDialog.open()
                        }
                    }
                    Rectangle {
                        implicitHeight: 44
                        implicitWidth: createLbl.width + 36
                        radius: 8
                        color: createMa.pressed ? "#333" : "#1a1a1a"
                        visible: appController.currentUserRole === "admin" || appController.currentUserRole === "deputy"
                        Text {
                            id: createLbl
                            anchors.centerIn: parent
                            text: qsTr("+ Создать группу")
                            color: "#fff"
                            font.bold: true
                        }
                        MouseArea {
                            id: createMa
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: createGroupDialog.open()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: portalSyncRow.implicitHeight + 16
                    radius: 8
                    color: "#eff6ff"
                    border.color: "#bfdbfe"
                    visible: appController.portalSyncBusy
                    ColumnLayout {
                        id: portalSyncRow
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6
                        Label {
                            Layout.fillWidth: true
                            text: appController.portalSyncMessage
                            font.pixelSize: 12
                            color: "#1e40af"
                            wrapMode: Text.WordWrap
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: appController.portalSyncPercent
                        }
                    }
                }

                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    implicitHeight: 44
                    leftPadding: 40
                    placeholderText: qsTr("Поиск по номеру группы…")
                    onTextChanged: { root.searchText = text; syncGroupList() }
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: "🔍"
                        opacity: 0.45
                    }
                }

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
                        spacing: 14
                        model: groupList
                        clip: true

                        delegate: Rectangle {
                            width: listView.width
                            height: innerCol.implicitHeight + 28
                            radius: 10
                            color: groupId === root.selectedGroupId ? "#eff6ff" : "#ffffff"
                            border.color: groupId === root.selectedGroupId ? "#93c5fd" : "#e5e7eb"
                            border.width: 1

                            ColumnLayout {
                                id: innerCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 16
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 14
                                    Rectangle {
                                        width: 44; height: 44; radius: 10; color: "#dbeafe"
                                        Text { anchors.centerIn: parent; text: "👥"; font.pixelSize: 20 }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Text {
                                            text: number
                                            font.bold: true
                                            font.pixelSize: 16
                                            Layout.fillWidth: true
                                        }
                                        Text {
                                            visible: subtitle && subtitle.length > 0
                                            text: subtitle
                                            font.pixelSize: 12
                                            color: "#4b5563"
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                        RowLayout {
                                            spacing: 16
                                            Text { text: qsTr("Студентов: ") + studentCount; font.pixelSize: 12; color: "#6b7280" }
                                            Text { text: qsTr("Ведомостей: ") + sheetCount; font.pixelSize: 12; color: "#6b7280" }
                                            Text { text: "📅 " + formatDate(createdAt); font.pixelSize: 12; color: "#6b7280" }
                                        }
                                    }
                                    Rectangle {
                                        radius: 13
                                        color: studentCount > 0 ? "#2e7d32" : "#9e9e9e"
                                        implicitHeight: 26
                                        implicitWidth: pillTxt.width + 20
                                        Text {
                                            id: pillTxt
                                            anchors.centerIn: parent
                                            color: "#fff"
                                            font.pixelSize: 11
                                            font.bold: true
                                            text: studentCount > 0 ? qsTr("Активна") : qsTr("Пустая")
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 18
                                    Text {
                                        text: qsTr("👥 Студенты")
                                        font.pixelSize: 12
                                        color: "#2563eb"
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.openGroupStudents(groupId, number)
                                        }
                                    }
                                    Text {
                                        text: qsTr("📋 Ведомости")
                                        font.pixelSize: 12
                                        color: "#7c3aed"
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.openDisciplines(groupId, number)
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: qsTr("✕ Удалить")
                                        font.pixelSize: 12
                                        color: "#dc2626"
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                deleteGroupDlg.groupId = groupId
                                                deleteGroupDlg.open()
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                z: -1
                                onClicked: root.selectedGroupId = groupId
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: groupList.count === 0
                            text: qsTr("Групп пока нет.\nСохраните страницы групп с портала в папку\n(кнопка «⟳ Импорт групп») или создайте вручную.")
                            horizontalAlignment: Text.AlignHCenter
                            color: "#6b7280"
                        }
                    }
                }
            }
        }
    }

    StackView {
        id: detailStack
        anchors.fill: parent
        z: 1000
        clip: true
        focus: true
        visible: depth > 0
        Keys.onEscapePressed: if (depth > 0) pop()

        popEnter: null
        popExit: null
        pushEnter: null
        pushExit: null
    }

    Component {
        id: groupPageCmp
        GroupPage {
            onBack: detailStack.pop()
            onOpenDisciplines: function(gid, gnum) {
                root.pushDetail(disciplinesCmp, { "groupId": gid, "groupNumber": gnum })
            }
        }
    }

    Component {
        id: disciplinesCmp
        DisciplinesPage {
            onBack: detailStack.pop()
            onOpenGradeSheet: function(sid, gid, gnum) {
                root.openGradeSheet(sid, gid, gnum)
            }
        }
    }

    Component {
        id: gradeSheetCmp
        GradeSheetPage {
            onBack: detailStack.pop()
        }
    }

    Component {
        id: approvalPageCmp
        ApprovalPage {
            onBack: detailStack.pop()
            onOpenSheet: function(sid, gid, gnum) {
                root.openGradeSheet(sid, gid, gnum)
            }
        }
    }

    Dialog {
        id: portalSyncDialog
        title: qsTr("Настройки импорта групп")
        modal: true
        standardButtons: Dialog.Close
        anchors.centerIn: parent
        width: 560
        height: 720

        onAboutToShow: {
            portalLoginField.text = appController.portalSavedLogin
            portalIndexField.text = appController.portalGroupsIndexUrl
        }

        ScrollView {
            anchors.fill: parent
            contentWidth: availableWidth
            
            ColumnLayout {
                width: parent.width
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: qsTr("📁 Импорт из HTML файлов (без логина)")
                    font.bold: true
                    font.pixelSize: 13
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("В браузере откройте страницу группы на portal.novsu.ru → Ctrl+S → сохраните HTML в папку ниже. При входе в приложение группы импортируются автоматически.")
                    font.pixelSize: 11
                    color: "#555"
                }

                Label {
                    Layout.fillWidth: true
                    text: appController.portalLocalImportDir
                    font.pixelSize: 10
                    color: "#888"
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Button {
                        text: qsTr("Открыть папку")
                        Layout.fillWidth: true
                        onClicked: appController.openPortalImportFolder()
                    }
                    Button {
                        text: qsTr("Импортировать сейчас")
                        Layout.fillWidth: true
                        highlighted: true
                        enabled: !appController.portalSyncBusy
                        onClicked: appController.importGroupsFromLocalHtml()
                    }
                }

                CheckBox {
                    id: autoImportChk
                    text: qsTr("Автоимпорт при входе и при новых файлах")
                    checked: appController.portalAutoImport
                    onCheckedChanged: appController.portalAutoImport = checked
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#ddd"
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("🔍 Автоматический поиск групп")
                    font.bold: true
                    font.pixelSize: 13
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Автоматически проверяет существование групп по номерам. Требуется сохраненная сессия портала.")
                    font.pixelSize: 11
                    color: "#555"
                }

                TextField {
                    id: searchPatternsField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Примеры: 2*, 29*, 2991, 2900-2950 или пусто для умного поиска")
                    text: ""
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    Button {
                        text: qsTr("🔍 Поиск по паттернам")
                        Layout.fillWidth: true
                        highlighted: true
                        enabled: !appController.portalSyncBusy && appController.portalHasSavedSession && appController.isOnline
                        onClicked: {
                            var patterns = searchPatternsField.text.trim()
                            if (patterns.length > 0) {
                                var patternList = patterns.split(',').map(function(p) { return p.trim() })
                                appController.autoSearchGroups(patternList)
                            } else {
                                appController.autoSearchGroups([])
                            }
                        }
                    }
                    
                    Button {
                        text: qsTr("🤖 Умный поиск")
                        Layout.fillWidth: true
                        enabled: !appController.portalSyncBusy && appController.portalHasSavedSession && appController.isOnline
                        onClicked: {
                            // Автопоиск с паттернами по умолчанию (текущий год ± 3)
                            appController.autoSearchGroups([])
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("💡 Примеры паттернов:\n• 2* — все группы 2100-2999\n• 29* — группы 2900-2999\n• 2991 — только группа 2991\n• 2900-2950 — диапазон\n• Пусто (умный поиск) — группы 0100-9899 (все активные)")
                    font.pixelSize: 10
                    color: "#666"
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("⚠️ Требуется сохраненная сессия портала (войдите один раз с паролем)")
                    font.pixelSize: 10
                    color: "#dc2626"
                    wrapMode: Text.WordWrap
                    visible: !appController.portalHasSavedSession
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#ddd"
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("🌐 Онлайн синхронизация (логин нужен один раз)")
                    font.bold: true
                    font.pixelSize: 13
                }

                TextField {
                    id: portalLoginField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Логин портала")
                }
                TextField {
                    id: portalPasswordField
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Пароль (только при первом входе)")
                }
                TextField {
                    id: portalIndexField
                    Layout.fillWidth: true
                    placeholderText: qsTr("URL списка групп — скопируйте из браузера")
                    onTextChanged: appController.portalGroupsIndexUrl = text
                }
                CheckBox {
                    id: rememberPortalSessionChk
                    text: qsTr("Запомнить сессию портала")
                    checked: true
                }
                CheckBox {
                    text: qsTr("Автозагрузка с портала при входе")
                    checked: appController.portalOnlineAutoSync
                    onCheckedChanged: appController.portalOnlineAutoSync = checked
                }
                
                Label {
                    Layout.fillWidth: true
                    text: qsTr("ℹ️ При автозагрузке импортируются только активные группы текущего семестра")
                    font.pixelSize: 10
                    color: "#0066cc"
                    wrapMode: Text.WordWrap
                    visible: appController.portalOnlineAutoSync
                }
                
                Label {
                    Layout.fillWidth: true
                    text: qsTr("⏰ Автоматическое обновление: раз в 24 часа")
                    font.pixelSize: 10
                    color: "#0066cc"
                    wrapMode: Text.WordWrap
                    visible: appController.portalOnlineAutoSync
                }
                
                Button {
                    Layout.fillWidth: true
                    text: appController.portalHasSavedSession
                          ? qsTr("Обновить с портала (без пароля)")
                          : qsTr("Войти и загрузить группы")
                    highlighted: true
                    enabled: appController.isOnline && !appController.portalSyncBusy
                             && portalIndexField.text.length > 0
                             && (appController.portalHasSavedSession
                                 || (portalLoginField.text.length > 0 && portalPasswordField.text.length > 0))
                    onClicked: {
                        appController.portalGroupsIndexUrl = portalIndexField.text
                        if (portalLoginField.text.length > 0)
                            appController.portalSavedLogin = portalLoginField.text
                        if (rememberPortalSessionChk.checked && portalPasswordField.text.length > 0) {
                            appController.syncGroupsFromPortal(portalLoginField.text, portalPasswordField.text)
                        } else if (appController.portalHasSavedSession) {
                            appController.syncPortalOnlineSavedSession()
                        } else {
                            appController.syncGroupsFromPortal(portalLoginField.text, portalPasswordField.text)
                        }
                        portalPasswordField.text = ""
                    }
                }
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Забыть сессию портала")
                    visible: appController.portalHasSavedSession
                    onClicked: appController.clearPortalSession()
                }
                
                Label {
                    Layout.fillWidth: true
                    text: appController.portalSyncMessage
                    font.pixelSize: 11
                    color: "#666"
                    wrapMode: Text.WordWrap
                    visible: appController.portalSyncBusy
                }
                
                ProgressBar {
                    Layout.fillWidth: true
                    value: appController.portalSyncPercent / 100.0
                    visible: appController.portalSyncBusy
                }
            }
        }
    }

    Dialog {
        id: createGroupDialog
        title: qsTr("Новая учебная группа")
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        anchors.centerIn: parent
        width: 400
        ColumnLayout {
            spacing: 8
            Label { text: qsTr("Номер группы") }
            TextField {
                id: newGroupNumber
                Layout.fillWidth: true
                placeholderText: qsTr("ИВТ-101")
            }
        }
        onAccepted: {
            if (appController.createGroup(newGroupNumber.text))
                newGroupNumber.text = ""
            syncGroupList()
        }
    }

    Dialog {
        id: deleteGroupDlg
        property string groupId: ""
        title: qsTr("Удалить группу?")
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        Label { text: qsTr("Будут удалены все студенты и ведомости.") }
        onAccepted: {
            appController.deleteGroup(deleteGroupDlg.groupId)
            syncGroupList()
        }
    }

    Dialog {
        id: manageAccountsDialog
        title: qsTr("Управление учётными записями")
        modal: true
        anchors.centerIn: parent
        width: Math.min(500, parent.width - 60)

        ColumnLayout {
            spacing: 12
            Label { text: qsTr("Создать учётную запись"); font.bold: true; font.pixelSize: 16 }
            TextField { id: mgLogin; Layout.fillWidth: true; placeholderText: qsTr("Логин") }
            TextField { id: mgName; Layout.fillWidth: true; placeholderText: qsTr("Отображаемое имя") }
            TextField { id: mgPass; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: qsTr("Пароль") }
            TextField { id: mgPass2; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: qsTr("Повтор пароля") }

            Label { text: qsTr("Роль"); font.bold: true }
            ComboBox {
                id: mgRole
                Layout.fillWidth: true
                model: [qsTr("Преподаватель"), qsTr("Заместитель директора"), qsTr("Администратор")]
                property var roleValues: ["teacher", "deputy", "admin"]
                property string selectedRole: roleValues[currentIndex]
            }

            // Подсказка по роли
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: roleHintText.implicitHeight + 16
                radius: 8
                color: "#f0f9ff"
                border.color: "#bae6fd"
                border.width: 1
                Text {
                    id: roleHintText
                    anchors { fill: parent; margins: 10 }
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                    color: "#0369a1"
                    text: {
                        if (mgRole.currentIndex === 0)
                            return qsTr("Преподаватель: создаёт ведомости и отправляет на согласование заместителю директора.")
                        if (mgRole.currentIndex === 1)
                            return qsTr("Заместитель директора: создаёт ведомости и подтверждает (согласовывает) записи.")
                        return qsTr("Администратор: полный доступ ко всем функциям системы.")
                    }
                }
            }

            Text { id: mgErr; Layout.fillWidth: true; visible: text.length > 0; color: "#dc2626"; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: qsTr("Отмена")
                    onClicked: manageAccountsDialog.close()
                }
                Button {
                    text: qsTr("Создать")
                    highlighted: true
                    Layout.fillWidth: true
                    onClicked: {
                        mgErr.text = ""
                        if (mgPass.text !== mgPass2.text) {
                            mgErr.text = qsTr("Пароли не совпадают.")
                            return
                        }
                        if (!authController.registerUser(mgLogin.text, mgPass.text, mgName.text, mgRole.selectedRole))
                            mgErr.text = authController.lastError
                        else {
                            mgLogin.text = ""
                            mgName.text = ""
                            mgPass.text = ""
                            mgPass2.text = ""
                            mgRole.currentIndex = 0
                            mgErr.text = qsTr("✓ Пользователь создан успешно!")
                        }
                    }
                }
            }
        }
    }
}

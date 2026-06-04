# Листинги кода для документации диплома

Подборка ключевых фрагментов кода с пояснениями для включения в дипломную работу.

---

## Листинг 1. Точка входа приложения (main.cpp)

**Назначение:** Инициализация Qt приложения и подключение контроллеров к QML интерфейсу.

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "app/AppController.h"
#include "auth/AuthController.h"

using namespace std;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("offline-edm"));
    QGuiApplication::setOrganizationName(QStringLiteral("offline-edm"));

    QQmlApplicationEngine engine;

    // Создание контроллеров
    AuthController authController;
    AppController appController;
    
    // Регистрация контроллеров для доступа из QML
    engine.rootContext()->setContextProperty(
        QStringLiteral("authController"), &authController
    );
    engine.rootContext()->setContextProperty(
        QStringLiteral("appController"), &appController
    );

    // Загрузка главного QML файла
    const QUrl url(QStringLiteral("qrc:/ui/qml/App.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );

    engine.load(url);
    return app.exec();
}
```

**Пояснение:** Функция main создает приложение Qt, регистрирует C++ контроллеры для использования в QML интерфейсе и загружает главное окно приложения.

---

## Листинг 2. Интерфейс контроллера аутентификации (AuthController.h)

**Назначение:** Управление сессиями пользователей и регистрация новых учетных записей.

```cpp
#pragma once

#include <QObject>
#include <QString>

class AuthController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isAuthenticated READ isAuthenticated NOTIFY sessionChanged)
    Q_PROPERTY(QString currentLogin READ currentLogin NOTIFY sessionChanged)
    Q_PROPERTY(QString currentRole READ currentRole NOTIFY sessionChanged)

public:
    explicit AuthController(QObject* parent = nullptr);

    bool isAuthenticated() const { return !m_login.isEmpty(); }
    QString currentLogin() const { return m_login; }
    QString currentRole() const { return m_role; }

    // Методы, доступные из QML
    Q_INVOKABLE bool registerUser(const QString& login, 
                                  const QString& password,
                                  const QString& displayName, 
                                  const QString& role = "teacher");
    
    Q_INVOKABLE bool login(const QString& login, const QString& password);
    Q_INVOKABLE void logout();

    void ensureDefaultAdmin();

signals:
    void sessionChanged();
    void lastErrorChanged();

private:
    bool ensureAccountsDb();
    static QByteArray deriveKey(const QString& password, const QByteArray& salt);
    
    QString m_accountsDbPath;
    QString m_login;
    QString m_displayName;
    QString m_role;
    QString m_lastError;
};
```

**Пояснение:** Контроллер использует механизм Q_PROPERTY для связывания данных с QML интерфейсом. Методы помечены Q_INVOKABLE для вызова из JavaScript кода.

---

## Листинг 3. Интерфейс репозитория академических данных (AcademicRepository.h)

**Назначение:** Управление учебными группами, студентами и ведомостями в SQLite базе.

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "../domain/AcademicTypes.h"
#include "../portal/PortalTypes.h"

class AcademicRepository : public QObject
{
    Q_OBJECT

public:
    explicit AcademicRepository(const QString& dbPath, QObject* parent = nullptr);
    ~AcademicRepository() override;

    // Управление группами
    QVector<StudyGroup> listGroups() const;
    bool createGroup(const QString& number);
    bool deleteGroup(const QString& groupId);
    QString findGroupIdByNumber(const QString& number) const;
    
    // Импорт из портала НовГУ
    QString upsertGroupFromPortal(
        const QString& number, 
        const QString& admissionYear,
        const QString& course,
        const QString& specialty, 
        const QString& profile,
        const QString& institute,
        const QString& studyForm, 
        const QString& portalUrl
    );
    
    int mergePortalStudents(
        const QString& groupId, 
        const QList<PortalStudentRow>& students
    );

    // Управление студентами
    QVector<Student> listStudents(const QString& groupId) const;
    bool addStudent(const QString& groupId, 
                   const QString& studentNumber,
                   const QString& fio, 
                   const QString& status);
    bool updateStudent(const QString& studentId, 
                      const QString& fio,
                      const QString& status, 
                      const QString& grade);

    // Управление ведомостями
    QVector<GradeSheet> listGradeSheets(const QString& groupId) const;
    bool createGradeSheet(const QString& groupId, 
                         const QString& disciplineId,
                         const QString& examType, 
                         QString* outSheetId = nullptr);
    bool setGradeSheetStatus(const QString& sheetId, const QString& status);

private:
    bool open();
    bool ensureSchema();

    QString m_dbPath;
    QString m_connectionName;
};
```

**Пояснение:** Репозиторий реализует паттерн Repository для работы с базой данных. Все операции инкапсулированы в методах класса.

---

## Листинг 4. Парсинг HTML портала НовГУ (фрагмент NovsuHtmlParser.cpp)

**Назначение:** Извлечение данных о студентах из HTML страниц портала.

```cpp
bool NovsuHtmlParser::parseGroupPage(const QString& html, 
                                     const QUrl& pageUrl,
                                     PortalGroupDetail* out)
{
    if (!out) return false;

    PortalGroupDetail g;
    g.sourceUrl = pageUrl.toString(QUrl::RemoveFragment);

    // Извлекаем номер группы
    g.number = fieldFromHtml(html, QStringLiteral("Группа"));
    
    // Извлекаем метаданные
    g.admissionYear = fieldFromHtml(html, QStringLiteral("Год поступления"));
    g.course = fieldFromHtml(html, QStringLiteral("Курс"));
    g.specialty = fieldFromHtml(html, QStringLiteral("Направление"));
    g.institute = fieldFromHtml(html, QStringLiteral("Институт"));
    g.studyForm = fieldFromHtml(html, QStringLiteral("Форма обучения"));

    // Парсим таблицу студентов (№, ФИО, Статус)
    QRegularExpression rowRe(
        QStringLiteral("<tr\\b[^>]*>\\s*"
                      "<td[^>]*>\\s*(\\d+)\\s*</td>\\s*"
                      "<td[^>]*>\\s*(?:<a\\b[^>]*>)?\\s*([^<]+?)\\s*(?:</a>)?\\s*</td>\\s*"
                      "<td[^>]*>\\s*([^<]+?)\\s*</td>\\s*"
                      "</tr>"),
        QRegularExpression::CaseInsensitiveOption
    );
    
    auto rit = rowRe.globalMatch(html);
    while (rit.hasNext()) {
        const auto m = rit.next();
        PortalStudentRow row;
        row.seq = m.captured(1).trimmed();
        row.fio = normalizeWs(decodeEntities(m.captured(2)));
        row.status = mapPortalStatus(decodeEntities(m.captured(3)));
        
        if (!row.fio.isEmpty()) {
            g.students.append(row);
        }
    }

    *out = move(g);
    return !g.number.isEmpty();
}
```

**Пояснение:** Функция использует регулярные выражения для парсинга HTML структуры и извлечения данных о группе и студентах.

---

## Листинг 5. Сервис шифрования данных (EncryptionService.h)

**Назначение:** Шифрование локальных данных для обеспечения безопасности.

```cpp
#pragma once

#include <QObject>
#include <QByteArray>

class KeyManager;

class EncryptionService : public QObject
{
    Q_OBJECT

public:
    explicit EncryptionService(const KeyManager& keyManager, 
                              QObject* parent = nullptr);
    ~EncryptionService() override;

    // Шифрует данные и добавляет аутентификацию (HMAC)
    // Формат: [version(1)][iv(16)][ciphertext(n)][mac(32)]
    QByteArray encrypt(const QByteArray& plaintext, 
                      const QByteArray& aad = {}) const;

    // Расшифровывает и проверяет аутентичность
    // Возвращает пустой массив при ошибке
    QByteArray decrypt(const QByteArray& packet, 
                      const QByteArray& aad = {}) const;

private:
    QByteArray m_keyEnc;  // Ключ для шифрования
    QByteArray m_keyMac;  // Ключ для HMAC
};
```

**Пояснение:** Сервис использует симметричное шифрование с аутентификацией сообщений (HMAC) для защиты данных.

---

## Листинг 6. Структура данных учебной группы (AcademicTypes.h)

**Назначение:** Определение доменных типов для работы с академическими данными.

```cpp
struct StudyGroup
{
    QString id;              // Уникальный идентификатор
    QString number;          // Номер группы (например, 2991)
    qint64 createdAt;        // Временная метка создания
    int studentCount;        // Количество студентов
    int sheetCount;          // Количество ведомостей
    
    // Метаданные из портала
    QString admissionYear;   // Год поступления
    QString course;          // Курс обучения
    QString specialty;       // Направление подготовки
    QString institute;       // Институт
    QString studyForm;       // Форма обучения
    QString subtitle;        // Подзаголовок для отображения
};

struct Student
{
    QString id;              // Уникальный идентификатор
    QString groupId;         // ID группы
    QString studentNumber;   // Номер студента
    QString fio;            // ФИО
    QString status;         // Статус (обучается, отчислен и т.д.)
    QString grade;          // Оценка
};

struct GradeSheet
{
    QString id;             // Уникальный идентификатор
    QString groupId;        // ID группы
    QString disciplineId;   // ID дисциплины
    QString title;          // Название ведомости
    qint64 createdAt;       // Временная метка создания
    QString examType;       // Тип: "exam" или "credit"
    QString status;         // Статус: "draft", "pending_approval", "approved"
    QString disciplineName; // Название дисциплины
    QString groupNumber;    // Номер группы
    QString approvedBy;     // Кто утвердил
};
```

**Пояснение:** Структуры данных описывают бизнес-сущности системы и используются для передачи данных между слоями приложения.

---

## Листинг 7. CMake конфигурация проекта (CMakeLists.txt, фрагмент)

**Назначение:** Описание процесса сборки приложения с использованием CMake.

```cmake
cmake_minimum_required(VERSION 3.22)

project(offline-edm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(OFFLINE_EDM_BUILD_TESTS "Build unit tests" OFF)

# Поиск необходимых компонентов Qt6
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Gui 
    Qml 
    Quick 
    Network 
    Sql
)

# Создание исполняемого файла
qt_add_executable(offline-edm
    src/main.cpp
    src/auth/AuthController.cpp
    src/auth/AuthController.h
    src/app/AppController.cpp
    src/app/AppController.h
    src/storage/AcademicRepository.cpp
    src/storage/AcademicRepository.h
    src/portal/NovsuHtmlParser.cpp
    src/portal/NovsuHtmlParser.h
    # ... другие файлы
    resources.qrc
)

# Линковка с библиотеками Qt
target_link_libraries(offline-edm PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::Sql
)

# Поддержка тестов
if (OFFLINE_EDM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

**Пояснение:** CMake автоматизирует процесс сборки, управляет зависимостями и поддерживает кросс-платформенную компиляцию.

---

## Листинг 8. QML интерфейс страницы входа (LoginPage.qml, фрагмент)

**Назначение:** Декларативное описание пользовательского интерфейса.

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "Вход в систему"
    
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20
        width: 300
        
        Label {
            text: "Система электронного документооборота"
            font.pixelSize: 18
            Layout.alignment: Qt.AlignHCenter
        }
        
        TextField {
            id: loginField
            placeholderText: "Логин"
            Layout.fillWidth: true
        }
        
        TextField {
            id: passwordField
            placeholderText: "Пароль"
            echoMode: TextField.Password
            Layout.fillWidth: true
        }
        
        Button {
            text: "Войти"
            Layout.fillWidth: true
            onClicked: {
                if (authController.login(loginField.text, passwordField.text)) {
                    stackView.push("GroupsPage.qml")
                } else {
                    errorDialog.open()
                }
            }
        }
    }
}
```

**Пояснение:** QML позволяет описывать интерфейс декларативно, обращаясь к C++ контроллерам через привязку свойств.

---

## Листинг 9. Юнит-тест парсера HTML (TestHtmlParser.cpp, фрагмент)

**Назначение:** Автоматическое тестирование функциональности парсинга.

```cpp
#include <QtTest>
#include "../src/portal/NovsuHtmlParser.h"

class TestHtmlParser : public QObject
{
    Q_OBJECT

private slots:
    void testParseGroupPage()
    {
        const QString html = R"(
            <html>
                <body>
                    <div>Группа: 2991</div>
                    <div>Курс: 4</div>
                    <table>
                        <tr>
                            <td>1</td>
                            <td>Иванов Иван Иванович</td>
                            <td>обучается</td>
                        </tr>
                        <tr>
                            <td>2</td>
                            <td>Петров Петр Петрович</td>
                            <td>обучается</td>
                        </tr>
                    </table>
                </body>
            </html>
        )";

        PortalGroupDetail result;
        const bool ok = NovsuHtmlParser::parseGroupPage(
            html, 
            QUrl("https://portal.novsu.ru/group.html"), 
            &result
        );

        QVERIFY(ok);
        QCOMPARE(result.number, QString("2991"));
        QCOMPARE(result.course, QString("4"));
        QCOMPARE(result.students.size(), 2);
        QCOMPARE(result.students[0].fio, QString("Иванов Иван Иванович"));
    }
};

QTEST_MAIN(TestHtmlParser)
#include "TestHtmlParser.moc"
```

**Пояснение:** Qt Test Framework позволяет писать автоматические тесты для проверки корректности кода.

---

## Рекомендации по использованию в документации

### Для общей части диплома:
- **Листинг 1** - в разделе об архитектуре приложения
- **Листинг 6** - в разделе о модели данных
- **Листинг 7** - в разделе об инструментах разработки

### Для специальной части:
- **Листинг 2** - в разделе об аутентификации пользователей
- **Листинг 3** - в разделе о хранении данных
- **Листинг 4** - в разделе об интеграции с порталом НовГУ
- **Листинг 5** - в разделе о безопасности данных
- **Листинг 8** - в разделе о пользовательском интерфейсе
- **Листинг 9** - в разделе о тестировании

### Советы:
1. Добавляйте заголовки "Листинг X.Y - Название" перед каждым фрагментом
2. После кода приводите краткое пояснение на 2-3 предложения
3. Указывайте имя файла и путь к нему в проекте
4. Выделяйте ключевые строки кода жирным или курсивом в пояснениях
5. Используйте моноширинный шрифт для кода (обычно Courier New 10pt)

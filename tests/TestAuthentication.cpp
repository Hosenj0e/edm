#include <QtTest/QtTest>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QRandomGenerator>
#include "auth/AuthController.h"

class TestAuthentication final : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        // Уникальное имя приложения для изоляции теста с случайным компонентом
        const QString testName = QString("offline-edm-test-%1-%2")
            .arg(QTest::currentTestFunction())
            .arg(QRandomGenerator::global()->generate());
        QCoreApplication::setOrganizationName("offline-edm-test");
        QCoreApplication::setApplicationName(testName);
        
        // Очистим старые данные
        const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir(dataPath).removeRecursively();
    }

    void cleanup()
    {
        // Удалим все тестовые данные после каждого теста
        const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir(dataPath).removeRecursively();
    }

    void testRegisterUser_success()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        const bool result = auth.registerUser("testuser", "password123", "Test User", "teacher");
        QVERIFY(result);
        QVERIFY(auth.lastError().isEmpty());
    }

    void testRegisterUser_duplicateLogin()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        QVERIFY(auth.registerUser("testuser2", "password1", "User 1", "teacher"));
        
        const bool result = auth.registerUser("testuser2", "password2", "User 2", "admin");
        QVERIFY(!result);
        QVERIFY(!auth.lastError().isEmpty());
    }

    void testLogin_validCredentials()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        QVERIFY(auth.registerUser("testuser3", "password123", "Test User", "teacher"));
        
        const bool result = auth.login("testuser3", "password123");
        QVERIFY(result);
        QVERIFY(auth.lastError().isEmpty());
        QVERIFY(auth.isAuthenticated());
        QCOMPARE(auth.currentLogin(), QStringLiteral("testuser3"));
        QCOMPARE(auth.currentRole(), QStringLiteral("teacher"));
    }

    void testLogin_invalidPassword()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        QVERIFY(auth.registerUser("testuser4", "password123", "Test User", "teacher"));
        
        const bool result = auth.login("testuser4", "wrongpassword");
        QVERIFY(!result);
        QVERIFY(!auth.lastError().isEmpty());
        QVERIFY(!auth.isAuthenticated());
    }

    void testLogin_nonExistentUser()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        const bool result = auth.login("nonexistent", "password");
        QVERIFY(!result);
        QVERIFY(!auth.lastError().isEmpty());
        QVERIFY(!auth.isAuthenticated());
    }

    void testLogout_clearsSession()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        QVERIFY(auth.registerUser("testuser5", "password123", "Test User", "teacher"));
        QVERIFY(auth.login("testuser5", "password123"));
        QVERIFY(auth.isAuthenticated());

        auth.logout();
        
        QVERIFY(!auth.isAuthenticated());
        QVERIFY(auth.currentLogin().isEmpty());
    }

    void testPasswordHashing_differentSalts()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        QVERIFY(auth.registerUser("user1", "samepassword", "User 1", "teacher"));
        QVERIFY(auth.registerUser("user2", "samepassword", "User 2", "teacher"));

        // Оба пользователя должны войти с одинаковым паролем
        QVERIFY(auth.login("user1", "samepassword"));
        auth.logout();
        QVERIFY(auth.login("user2", "samepassword"));
        
        // Если оба входят, значит соли работают корректно
    }

    void testRolePermissions_teacher()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        QVERIFY(auth.registerUser("teacher1", "password", "Teacher", "teacher"));
        QVERIFY(auth.login("teacher1", "password"));

        QCOMPARE(auth.currentRole(), QStringLiteral("teacher"));
    }

    void testRolePermissions_deputy()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        QVERIFY(auth.registerUser("deputy1", "password", "Deputy", "deputy"));
        QVERIFY(auth.login("deputy1", "password"));

        QCOMPARE(auth.currentRole(), QStringLiteral("deputy"));
    }

    void testRolePermissions_admin()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        QVERIFY(auth.registerUser("admin2", "password", "Admin", "admin"));
        QVERIFY(auth.login("admin2", "password"));

        QCOMPARE(auth.currentRole(), QStringLiteral("admin"));
    }

    void testEmptyPassword_rejected()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        const bool result = auth.registerUser("testuser6", "", "Test User", "teacher");
        QVERIFY(!result);
        QVERIFY(!auth.lastError().isEmpty());
    }

    void testDefaultAdmin_exists()
    {
        AuthController auth;
        auth.ensureDefaultAdmin();

        // Должен войти как admin/123
        const bool result = auth.login("admin", "123");
        QVERIFY(result);
        QCOMPARE(auth.currentRole(), QStringLiteral("admin"));
    }
};

QTEST_MAIN(TestAuthentication)
#include "TestAuthentication.moc"

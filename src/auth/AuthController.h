#pragma once

#include <QObject>
#include <QString>

class AuthController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isAuthenticated READ isAuthenticated NOTIFY sessionChanged)
    Q_PROPERTY(QString currentLogin READ currentLogin NOTIFY sessionChanged)
    Q_PROPERTY(QString currentDisplayName READ currentDisplayName NOTIFY sessionChanged)
    Q_PROPERTY(QString currentRole READ currentRole NOTIFY sessionChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit AuthController(QObject* parent = nullptr);

    bool isAuthenticated() const { return !m_login.isEmpty(); }
    QString currentLogin() const { return m_login; }
    QString currentDisplayName() const { return m_displayName; }
    QString currentRole() const { return m_role; }  // "admin" | "teacher" | "deputy"
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE QString activeUserDataPath() const;

    Q_INVOKABLE bool registerUser(const QString& login, const QString& password,
                                  const QString& displayName, const QString& role = QStringLiteral("teacher"));
    Q_INVOKABLE bool login(const QString& login, const QString& password);
    Q_INVOKABLE void logout();

    void ensureDefaultAdmin();

signals:
    void sessionChanged();
    void lastErrorChanged();

private:
    bool ensureAccountsDb();
    void setError(const QString& e);
    static QString normalizeLogin(const QString& login);
    static bool isValidLogin(const QString& login);
    static QByteArray randomSalt(int bytes);
    static QByteArray deriveKey(const QString& password, const QByteArray& salt);
    static bool slowEqual(const QByteArray& a, const QByteArray& b);

    QString m_accountsDbPath;
    QString m_usersRoot;
    QString m_login;
    QString m_displayName;
    QString m_role;
    QString m_lastError;
};

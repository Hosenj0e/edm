#include "AuthController.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include <QtNetwork/QPasswordDigestor>

namespace {
constexpr int Pbkdf2Iterations = 120000;
constexpr int KeyBytes = 32;
constexpr int SaltBytes = 16;
}

AuthController::AuthController(QObject* parent)
    : QObject(parent)
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/offline-edm");
    QDir().mkpath(root);
    m_accountsDbPath = root + QStringLiteral("/accounts.sqlite");
    m_usersRoot = root + QStringLiteral("/users");
    QDir().mkpath(m_usersRoot);
    ensureAccountsDb();
    ensureDefaultAdmin();
}

bool AuthController::ensureAccountsDb()
{
    const QString conn = QStringLiteral("offline-edm-accounts");
    if (!QSqlDatabase::contains(conn)) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(m_accountsDbPath);
        if (!db.open())
            return false;
    }
    auto db = QSqlDatabase::database(conn);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    return q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS users ("
        "login TEXT PRIMARY KEY NOT NULL,"
        "display_name TEXT NOT NULL,"
        "salt BLOB NOT NULL,"
        "password_hash BLOB NOT NULL,"
        "created_at INTEGER NOT NULL,"
        "role TEXT NOT NULL DEFAULT 'teacher'"
        ");"
    ));
}

void AuthController::setError(const QString& e)
{
    m_lastError = e;
    emit lastErrorChanged();
}

QString AuthController::normalizeLogin(const QString& login)
{
    const QString t = login.trimmed().toLower();
    QString out;
    out.reserve(t.size());
    for (QChar c : t) {
        if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-'))
            out.append(c);
        else if (c.isSpace())
            out.append(QLatin1Char('_'));
    }
    return out;
}

bool AuthController::isValidLogin(const QString& login)
{
    if (login.size() < 3 || login.size() > 48)
        return false;
    for (QChar c : login) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('_') && c != QLatin1Char('-'))
            return false;
    }
    return true;
}

QByteArray AuthController::randomSalt(int bytes)
{
    QByteArray out(bytes, char(0));
    for (int i = 0; i < bytes; ++i)
        out[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
    return out;
}

QByteArray AuthController::deriveKey(const QString& password, const QByteArray& salt)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                              password.toUtf8(),
                                              salt,
                                              Pbkdf2Iterations,
                                              KeyBytes);
}

bool AuthController::slowEqual(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size())
        return false;
    unsigned char d = 0;
    for (int i = 0; i < a.size(); ++i)
        d |= static_cast<unsigned char>(a[i] ^ b[i]);
    return d == 0;
}

QString AuthController::activeUserDataPath() const
{
    if (m_login.isEmpty())
        return {};
    return m_usersRoot + QLatin1Char('/') + m_login;
}

bool AuthController::registerUser(const QString& login, const QString& password, const QString& displayName, const QString& role)
{
    setError({});

    const QString norm = normalizeLogin(login);
    if (!isValidLogin(norm)) {
        setError(tr("Логин: 3–48 символов, только буквы, цифры, _ и -."));
        return false;
    }
    if (password.size() < 6) {
        setError(tr("Пароль не короче 6 символов."));
        return false;
    }
    const QString name = displayName.trimmed();
    if (name.isEmpty()) {
        setError(tr("Укажите отображаемое имя."));
        return false;
    }

    ensureAccountsDb();
    const QString conn = QStringLiteral("offline-edm-accounts");
    auto db = QSqlDatabase::database(conn);
    if (!db.isOpen()) {
        setError(tr("Не удалось открыть базу учётных записей."));
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT 1 FROM users WHERE login = :l"));
    q.bindValue(QStringLiteral(":l"), norm);
    if (!q.exec()) {
        setError(tr("Ошибка базы данных."));
        return false;
    }
    if (q.next()) {
        setError(tr("Такой логин уже занят."));
        return false;
    }

    const QByteArray salt = randomSalt(SaltBytes);
    const QByteArray hash = deriveKey(password, salt);
    const qint64 created = QDateTime::currentMSecsSinceEpoch();

    q.prepare(QStringLiteral(
        "INSERT INTO users (login, display_name, salt, password_hash, created_at, role) "
        "VALUES (:login, :dn, :salt, :ph, :created, :role)"));
    q.bindValue(QStringLiteral(":login"), norm);
    q.bindValue(QStringLiteral(":dn"), name);
    q.bindValue(QStringLiteral(":salt"), salt);
    q.bindValue(QStringLiteral(":ph"), hash);
    q.bindValue(QStringLiteral(":created"), created);
    q.bindValue(QStringLiteral(":role"), role.isEmpty() ? QStringLiteral("teacher") : role);
    if (!q.exec()) {
        setError(tr("Не удалось сохранить пользователя: %1").arg(q.lastError().text()));
        return false;
    }

    QDir().mkpath(m_usersRoot + QLatin1Char('/') + norm);
    m_login = norm;
    m_displayName = name;
    m_role = role.isEmpty() ? QStringLiteral("teacher") : role;
    emit sessionChanged();
    return true;
}

bool AuthController::login(const QString& login, const QString& password)
{
    const QString norm = normalizeLogin(login);
    if (norm.isEmpty() || password.isEmpty()) {
        logout(); // Очищаем сессию при ошибке (до setError!)
        setError(tr("Введите логин и пароль."));
        return false;
    }

    ensureAccountsDb();
    const QString conn = QStringLiteral("offline-edm-accounts");
    auto db = QSqlDatabase::database(conn);
    if (!db.isOpen()) {
        logout(); // Очищаем сессию при ошибке (до setError!)
        setError(tr("Не удалось открыть базу учётных записей."));
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT display_name, salt, password_hash, role FROM users WHERE login = :l"));
    q.bindValue(QStringLiteral(":l"), norm);
    if (!q.exec() || !q.next()) {
        logout(); // Очищаем сессию при ошибке (до setError!)
        setError(tr("Неверный логин или пароль."));
        return false;
    }

    const QString dn = q.value(0).toString();
    const QByteArray salt = q.value(1).toByteArray();
    const QByteArray stored = q.value(2).toByteArray();
    const QString role = q.value(3).toString();
    const QByteArray trial = deriveKey(password, salt);
    if (!slowEqual(trial, stored)) {
        logout(); // Очищаем сессию при ошибке (до setError!)
        setError(tr("Неверный логин или пароль."));
        return false;
    }

    setError({}); // Очищаем предыдущие ошибки только при успехе
    m_login = norm;
    m_displayName = dn;
    m_role = role.isEmpty() ? QStringLiteral("teacher") : role;
    QDir().mkpath(m_usersRoot + QLatin1Char('/') + m_login);
    emit sessionChanged();
    return true;
}

void AuthController::logout()
{
    m_login.clear();
    m_displayName.clear();
    m_role.clear();
    setError({});
    emit sessionChanged();
}

void AuthController::ensureDefaultAdmin()
{
    const QString conn = QStringLiteral("offline-edm-accounts");
    auto db = QSqlDatabase::database(conn);
    if (!db.isOpen())
        return;

    // Миграция: добавляем колонку role если её нет
    QSqlQuery mig(db);
    mig.exec(QStringLiteral("ALTER TABLE users ADD COLUMN role TEXT NOT NULL DEFAULT 'teacher'"));

    // Всегда принудительно выставляем роль admin для учётки admin
    QSqlQuery upd(db);
    upd.exec(QStringLiteral("UPDATE users SET role = 'admin' WHERE login = 'admin'"));

    // Создаём admin если не существует
    QSqlQuery check(db);
    check.prepare(QStringLiteral("SELECT 1 FROM users WHERE login = 'admin'"));
    if (!check.exec() || check.next())
        return; // уже существует — роль уже обновлена выше

    const QByteArray salt = randomSalt(SaltBytes);
    const QByteArray hash = deriveKey(QStringLiteral("123"), salt);
    const qint64 created = QDateTime::currentMSecsSinceEpoch();

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO users (login, display_name, salt, password_hash, created_at, role) "
        "VALUES ('admin', 'Администратор', :salt, :ph, :created, 'admin')"));
    ins.bindValue(QStringLiteral(":salt"), salt);
    ins.bindValue(QStringLiteral(":ph"), hash);
    ins.bindValue(QStringLiteral(":created"), created);
    ins.exec();

    QDir().mkpath(m_usersRoot + QStringLiteral("/admin"));
}

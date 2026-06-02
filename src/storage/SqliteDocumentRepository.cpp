#include "SqliteDocumentRepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDir>
#include <QUuid>
#include <QDateTime>
#include <QVariantMap>
#include <QDebug>

using namespace std;

static qint64 unixNowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

SqliteDocumentRepository::SqliteDocumentRepository(const QString& dbPath, QObject* parent)
    : QObject(parent)
    , m_dbPath(dbPath)
    , m_connectionName(QStringLiteral("offline-edm-sqlite-") + QString::number(reinterpret_cast<quintptr>(this)))
{
    openOrMigrate();
}

SqliteDocumentRepository::~SqliteDocumentRepository()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool SqliteDocumentRepository::openOrMigrate()
{
    // QSqlDatabase::addDatabase возвращает connection object.
    if (!QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(m_dbPath);
    }

    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.open()) {
        // Если драйвер не доступен/нет файла - пусть приложение решит дальше (для MVP).
        return false;
    }

    return ensureSchema();
}

bool SqliteDocumentRepository::ensureSchema()
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);

    // documents хранит метаданные + хэш содержимого (для подписи).
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS documents ("
            "doc_id TEXT PRIMARY KEY NOT NULL,"
            "title TEXT NOT NULL,"
            "status INTEGER NOT NULL,"
            "version INTEGER NOT NULL,"
            "created_at INTEGER NOT NULL,"
            "updated_at INTEGER NOT NULL,"
            "content_hash BLOB NOT NULL"
            ");"
            )))
        return false;

    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS approvals ("
            "doc_id TEXT NOT NULL,"
            "step_index INTEGER NOT NULL,"
            "approver_id TEXT NOT NULL,"
            "status INTEGER NOT NULL,"
            "updated_at INTEGER NOT NULL,"
            "PRIMARY KEY(doc_id, step_index)"
            ");"
            )))
        return false;

    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS signatures ("
            "signature_id TEXT PRIMARY KEY NOT NULL,"
            "doc_id TEXT NOT NULL,"
            "signer_id TEXT NOT NULL,"
            "signed_at INTEGER NOT NULL,"
            "signature BLOB NOT NULL"
            ");"
            )))
        return false;

    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS comments ("
            "comment_id TEXT PRIMARY KEY NOT NULL,"
            "doc_id TEXT NOT NULL,"
            "author_id TEXT NOT NULL,"
            "body TEXT NOT NULL,"
            "created_at INTEGER NOT NULL"
            ");"
            )))
        return false;

    return true;
}

QVector<DocumentSummary> SqliteDocumentRepository::listDocumentSummaries()
{
    QVector<DocumentSummary> out;
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    const QString sql = QStringLiteral(
        "SELECT doc_id, title, status, version, updated_at "
        "FROM documents ORDER BY updated_at DESC");

    if (!q.exec(sql))
        return out;

    while (q.next()) {
        DocumentSummary item;
        item.id = q.value(0).toString();
        item.title = q.value(1).toString();
        item.status = static_cast<DocumentStatus>(q.value(2).toInt());
        item.version = q.value(3).toInt();
        item.updatedAt = q.value(4).toLongLong();
        out.push_back(move(item));
    }

    return out;
}

bool SqliteDocumentRepository::createDocument(const QString& docId,
                                              const QString& title,
                                              DocumentStatus status,
                                              int version,
                                              qint64 createdAt,
                                              qint64 updatedAt,
                                              const QByteArray& contentHash,
                                              const QString& initialApproverId)
{
    qDebug() << "[SqliteDocumentRepository] Creating document:" << docId << "title:" << title;
    
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        qDebug() << "[SqliteDocumentRepository] ERROR: Database is not open!";
        return false;
    }

    QSqlQuery q(db);
    if (!db.transaction()) {
        qDebug() << "[SqliteDocumentRepository] ERROR: Failed to start transaction:" << db.lastError().text();
        return false;
    }

    q.prepare(QStringLiteral(
        "INSERT INTO documents (doc_id, title, status, version, created_at, updated_at, content_hash) "
        "VALUES (:doc_id, :title, :status, :version, :created_at, :updated_at, :content_hash)"));
    q.bindValue(QStringLiteral(":doc_id"), docId);
    q.bindValue(QStringLiteral(":title"), title);
    q.bindValue(QStringLiteral(":status"), static_cast<int>(status));
    q.bindValue(QStringLiteral(":version"), version);
    q.bindValue(QStringLiteral(":created_at"), createdAt);
    q.bindValue(QStringLiteral(":updated_at"), updatedAt);
    q.bindValue(QStringLiteral(":content_hash"), contentHash);

    if (!q.exec()) {
        qDebug() << "[SqliteDocumentRepository] ERROR: Failed to insert document:" << q.lastError().text();
        db.rollback();
        return false;
    }
    qDebug() << "[SqliteDocumentRepository] Document inserted successfully";

    q.prepare(QStringLiteral(
        "INSERT INTO approvals (doc_id, step_index, approver_id, status, updated_at) "
        "VALUES (:doc_id, :step_index, :approver_id, :status, :updated_at)"));
    q.bindValue(QStringLiteral(":doc_id"), docId);
    q.bindValue(QStringLiteral(":step_index"), 0);
    q.bindValue(QStringLiteral(":approver_id"), initialApproverId);
    q.bindValue(QStringLiteral(":status"), 0); // pending
    q.bindValue(QStringLiteral(":updated_at"), updatedAt);

    if (!q.exec()) {
        qDebug() << "[SqliteDocumentRepository] ERROR: Failed to insert approval:" << q.lastError().text();
        db.rollback();
        return false;
    }
    qDebug() << "[SqliteDocumentRepository] Approval inserted successfully";

    if (!db.commit()) {
        qDebug() << "[SqliteDocumentRepository] ERROR: Failed to commit transaction:" << db.lastError().text();
        return false;
    }
    
    qDebug() << "[SqliteDocumentRepository] SUCCESS: Document created and committed";
    return true;
}

QByteArray SqliteDocumentRepository::contentHashForDocument(const QString& docId) const
{
    QByteArray out;
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT content_hash FROM documents WHERE doc_id = :doc_id"));
    q.bindValue(QStringLiteral(":doc_id"), docId);
    if (!q.exec())
        return {};
    if (q.next())
        out = q.value(0).toByteArray();
    return out;
}

int SqliteDocumentRepository::versionForDocument(const QString& docId) const
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return 0;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT version FROM documents WHERE doc_id = :doc_id"));
    q.bindValue(QStringLiteral(":doc_id"), docId);
    if (!q.exec())
        return 0;
    if (q.next())
        return q.value(0).toInt();
    return 0;
}

bool SqliteDocumentRepository::approveDocument(const QString& docId, const QString& approverId, qint64 updatedAt)
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    if (!db.transaction())
        return false;

    q.prepare(QStringLiteral(
        "UPDATE approvals "
        "SET approver_id = :approver_id, status = 1, updated_at = :updated_at "
        "WHERE doc_id = :doc_id AND step_index = 0"));
    q.bindValue(QStringLiteral(":approver_id"), approverId);
    q.bindValue(QStringLiteral(":updated_at"), updatedAt);
    q.bindValue(QStringLiteral(":doc_id"), docId);
    if (!q.exec()) {
        db.rollback();
        return false;
    }

    q.prepare(QStringLiteral(
        "UPDATE documents "
        "SET status = :status, version = version + 1, updated_at = :updated_at "
        "WHERE doc_id = :doc_id"));
    q.bindValue(QStringLiteral(":status"), static_cast<int>(DocumentStatus::Approved));
    q.bindValue(QStringLiteral(":updated_at"), updatedAt);
    q.bindValue(QStringLiteral(":doc_id"), docId);
    if (!q.exec()) {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool SqliteDocumentRepository::signDocument(const QString& docId,
                                             const QString& signerId,
                                             const QByteArray& signature,
                                             qint64 signedAt,
                                             qint64 updatedAt)
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    if (!db.transaction())
        return false;

    const QString signatureId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    q.prepare(QStringLiteral(
        "INSERT INTO signatures (signature_id, doc_id, signer_id, signed_at, signature) "
        "VALUES (:signature_id, :doc_id, :signer_id, :signed_at, :signature)"));
    q.bindValue(QStringLiteral(":signature_id"), signatureId);
    q.bindValue(QStringLiteral(":doc_id"), docId);
    q.bindValue(QStringLiteral(":signer_id"), signerId);
    q.bindValue(QStringLiteral(":signed_at"), signedAt);
    q.bindValue(QStringLiteral(":signature"), signature);
    if (!q.exec()) {
        db.rollback();
        return false;
    }

    q.prepare(QStringLiteral(
        "UPDATE documents "
        "SET status = :status, version = version + 1, updated_at = :updated_at "
        "WHERE doc_id = :doc_id"));
    q.bindValue(QStringLiteral(":status"), static_cast<int>(DocumentStatus::Signed));
    q.bindValue(QStringLiteral(":updated_at"), updatedAt);
    q.bindValue(QStringLiteral(":doc_id"), docId);
    if (!q.exec()) {
        db.rollback();
        return false;
    }

    return db.commit();
}

DocumentStatus SqliteDocumentRepository::documentStatus(const QString& docId) const
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return DocumentStatus::Draft;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT status FROM documents WHERE doc_id = :doc_id"));
    q.bindValue(QStringLiteral(":doc_id"), docId);
    if (!q.exec() || !q.next())
        return DocumentStatus::Draft;
    return static_cast<DocumentStatus>(q.value(0).toInt());
}

bool SqliteDocumentRepository::updateDraftContent(const QString& docId, const QByteArray& contentHash, qint64 updatedAt)
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE documents SET content_hash = :h, updated_at = :t, version = version + 1 "
        "WHERE doc_id = :id AND status = :draft"));
    q.bindValue(QStringLiteral(":h"), contentHash);
    q.bindValue(QStringLiteral(":t"), updatedAt);
    q.bindValue(QStringLiteral(":id"), docId);
    q.bindValue(QStringLiteral(":draft"), static_cast<int>(DocumentStatus::Draft));
    return q.exec() && q.numRowsAffected() > 0;
}

QVariantList SqliteDocumentRepository::listComments(const QString& docId) const
{
    QVariantList out;
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT author_id, body, created_at FROM comments WHERE doc_id = :d ORDER BY created_at ASC"));
    q.bindValue(QStringLiteral(":d"), docId);
    if (!q.exec())
        return out;

    while (q.next()) {
        QVariantMap m;
        m.insert(QStringLiteral("author"), q.value(0).toString());
        m.insert(QStringLiteral("text"), q.value(1).toString());
        m.insert(QStringLiteral("createdAt"), q.value(2).toLongLong());
        out.push_back(m);
    }
    return out;
}

bool SqliteDocumentRepository::addComment(const QString& docId, const QString& authorId, const QString& body, qint64 createdAt)
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    const QString cid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    q.prepare(QStringLiteral(
        "INSERT INTO comments (comment_id, doc_id, author_id, body, created_at) "
        "VALUES (:cid, :doc, :author, :body, :ts)"));
    q.bindValue(QStringLiteral(":cid"), cid);
    q.bindValue(QStringLiteral(":doc"), docId);
    q.bindValue(QStringLiteral(":author"), authorId);
    q.bindValue(QStringLiteral(":body"), body);
    q.bindValue(QStringLiteral(":ts"), createdAt);
    return q.exec();
}


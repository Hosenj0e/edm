#include "SqliteSyncQueue.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QUuid>
#include <QDateTime>

using namespace std;

namespace {
qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}
}

SqliteSyncQueue::SqliteSyncQueue(const QString& dbPath, QObject* parent)
    : QObject(parent)
    , m_dbPath(dbPath)
    , m_connectionName(QStringLiteral("offline-edm-sync-queue-") + QString::number(reinterpret_cast<quintptr>(this)))
{
    open();
}

SqliteSyncQueue::~SqliteSyncQueue()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool SqliteSyncQueue::open()
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(m_dbPath);
    }

    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.open())
        return false;

    return ensureSchema();
}

bool SqliteSyncQueue::ensureSchema()
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);

    const QString sql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS sync_queue ("
        "task_id TEXT PRIMARY KEY NOT NULL,"
        "operation_type TEXT NOT NULL,"
        "doc_id TEXT NOT NULL,"
        "payload TEXT NOT NULL,"
        "created_at INTEGER NOT NULL,"
        "attempt_count INTEGER NOT NULL,"
        "status TEXT NOT NULL,"
        "last_error TEXT,"
        "updated_at INTEGER NOT NULL"
        ");");

    return q.exec(sql);
}

QString SqliteSyncQueue::enqueue(SyncOperationType operation, const QString& docId, const QJsonObject& payload)
{
    if (!QSqlDatabase::contains(m_connectionName))
        open();

    const QString taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 t = nowMs();

    QJsonDocument doc(payload);
    const QString payloadStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "INSERT INTO sync_queue (task_id, operation_type, doc_id, payload, created_at, attempt_count, status, updated_at) "
        "VALUES (:task_id, :operation_type, :doc_id, :payload, :created_at, 0, 'pending', :updated_at)"));
    q.bindValue(QStringLiteral(":task_id"), taskId);
    q.bindValue(QStringLiteral(":operation_type"), toString(operation));
    q.bindValue(QStringLiteral(":doc_id"), docId);
    q.bindValue(QStringLiteral(":payload"), payloadStr);
    q.bindValue(QStringLiteral(":created_at"), t);
    q.bindValue(QStringLiteral(":updated_at"), t);
    if (!q.exec())
        return {};

    return taskId;
}

QVector<SyncTask> SqliteSyncQueue::dequeuePending(int limit)
{
    QVector<SyncTask> out;
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT task_id, operation_type, doc_id, payload, created_at, attempt_count, status, updated_at "
        "FROM sync_queue "
        "WHERE status = 'pending' "
        "ORDER BY created_at ASC "
        "LIMIT :limit"));
    q.bindValue(QStringLiteral(":limit"), limit);
    if (!q.exec())
        return out;

    while (q.next()) {
        SyncTask task;
        task.taskId = q.value(0).toString();
        task.operation = operationFromString(q.value(1).toString());
        task.docId = q.value(2).toString();

        const QString payloadStr = q.value(3).toString();
        QJsonParseError err;
        const QJsonDocument payloadDoc = QJsonDocument::fromJson(payloadStr.toUtf8(), &err);
        if (!err.error && payloadDoc.isObject())
            task.payload = payloadDoc.object();

        task.createdAt = q.value(4).toLongLong();
        task.attemptCount = q.value(5).toInt();
        task.status = q.value(6).toString();
        task.updatedAt = q.value(7).toLongLong();
        out.push_back(move(task));
    }
    return out;
}

void SqliteSyncQueue::markSucceeded(const QString& taskId)
{
    const qint64 t = nowMs();
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "UPDATE sync_queue SET status = 'done', last_error = NULL, updated_at = :updated_at WHERE task_id = :task_id"));
    q.bindValue(QStringLiteral(":updated_at"), t);
    q.bindValue(QStringLiteral(":task_id"), taskId);
    q.exec();
}

void SqliteSyncQueue::markFailed(const QString& taskId, const QString& error)
{
    const qint64 t = nowMs();
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "UPDATE sync_queue "
        "SET attempt_count = attempt_count + 1, status = 'pending', last_error = :last_error, updated_at = :updated_at "
        "WHERE task_id = :task_id"));
    q.bindValue(QStringLiteral(":last_error"), error);
    q.bindValue(QStringLiteral(":updated_at"), t);
    q.bindValue(QStringLiteral(":task_id"), taskId);
    q.exec();
}

int SqliteSyncQueue::pendingCount() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return 0;

    QSqlQuery q(db);
    q.exec(QStringLiteral("SELECT COUNT(*) FROM sync_queue WHERE status = 'pending'"));
    if (q.next())
        return q.value(0).toInt();
    return 0;
}


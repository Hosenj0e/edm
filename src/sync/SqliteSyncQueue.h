#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QVector>

#include "SyncTypes.h"

class SqliteSyncQueue : public QObject
{
    Q_OBJECT

public:
    explicit SqliteSyncQueue(const QString& dbPath, QObject* parent = nullptr);
    ~SqliteSyncQueue() override;

    QString enqueue(SyncOperationType operation, const QString& docId, const QJsonObject& payload);
    QVector<SyncTask> dequeuePending(int limit);

    void markSucceeded(const QString& taskId);
    void markFailed(const QString& taskId, const QString& error);

    int pendingCount() const;

private:
    bool ensureSchema();
    bool open();

    QString m_dbPath;
    QString m_connectionName;
};


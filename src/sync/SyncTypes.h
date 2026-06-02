#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonValue>
#include <QByteArray>
#include <QVector>

enum class SyncOperationType : int {
    Create = 0,
    Approve = 1,
    Sign = 2
};

inline QString toString(SyncOperationType t)
{
    switch (t) {
    case SyncOperationType::Create:
        return QStringLiteral("create");
    case SyncOperationType::Approve:
        return QStringLiteral("approve");
    case SyncOperationType::Sign:
        return QStringLiteral("sign");
    }
    return QStringLiteral("unknown");
}

inline SyncOperationType operationFromString(const QString& s)
{
    if (s == QLatin1String("create"))
        return SyncOperationType::Create;
    if (s == QLatin1String("approve"))
        return SyncOperationType::Approve;
    if (s == QLatin1String("sign"))
        return SyncOperationType::Sign;
    return SyncOperationType::Create;
}

struct SyncTask
{
    QString taskId;
    SyncOperationType operation = SyncOperationType::Create;
    QString docId;
    QJsonObject payload;

    qint64 createdAt = 0;
    int attemptCount = 0;
    QString status; // pending/done/failed
    qint64 updatedAt = 0;
};


#include "RemoteSyncGateway.h"

#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>

static QJsonObject emptyRemoteState()
{
    return QJsonObject{
        {QStringLiteral("documents"), QJsonObject{}}
    };
}

LocalRemoteSyncGateway::LocalRemoteSyncGateway(const QString& remoteStatePath, QObject* parent)
    : QObject(parent)
    , m_remoteStatePath(remoteStatePath)
{
    QFile f(m_remoteStatePath);
    if (!f.exists()) {
        QDir().mkpath(QFileInfo(m_remoteStatePath).absolutePath());
        saveState(emptyRemoteState());
    }
}

QJsonObject LocalRemoteSyncGateway::loadState() const
{
    QFile f(m_remoteStatePath);
    if (!f.open(QIODevice::ReadOnly))
        return emptyRemoteState();

    const QByteArray data = f.readAll();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return emptyRemoteState();
    return doc.object();
}

bool LocalRemoteSyncGateway::saveState(const QJsonObject& state) const
{
    QFile f(m_remoteStatePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
    f.flush();
    return true;
}

bool LocalRemoteSyncGateway::applyOperation(SyncOperationType operation, const QJsonObject& payload)
{
    const QString docId = payload.value(QStringLiteral("docId")).toString();
    if (docId.isEmpty())
        return false;

    const int incomingVersion = payload.value(QStringLiteral("version")).toInt();
    const qint64 incomingUpdatedAt = payload.value(QStringLiteral("updatedAt")).toVariant().toLongLong();
    const int incomingStatus = payload.value(QStringLiteral("status")).toInt();
    const QString incomingTitle = payload.value(QStringLiteral("title")).toString();

    QJsonObject state = loadState();
    QJsonObject docs = state.value(QStringLiteral("documents")).toObject();

    const bool hasDoc = docs.contains(docId);
    if (!hasDoc) {
        if (operation != SyncOperationType::Create)
            return false;

        docs[docId] = QJsonObject{
            {QStringLiteral("title"), incomingTitle},
            {QStringLiteral("status"), incomingStatus},
            {QStringLiteral("version"), incomingVersion},
            {QStringLiteral("updatedAt"), QString::number(incomingUpdatedAt)}
        };

        state[QStringLiteral("documents")] = docs;
        return saveState(state);
    }

    const QJsonObject remoteDoc = docs.value(docId).toObject();
    const int remoteVersion = remoteDoc.value(QStringLiteral("version")).toInt();
    const qint64 remoteUpdatedAt =
        remoteDoc.value(QStringLiteral("updatedAt")).toVariant().toLongLong();

    const bool shouldUpdate =
        (incomingVersion > remoteVersion) ||
        (incomingVersion == remoteVersion && incomingUpdatedAt > remoteUpdatedAt);

    if (!shouldUpdate)
        return true; // Конфликт: применяем политику "игнорировать устаревшие изменения"

    docs[docId] = QJsonObject{
        {QStringLiteral("title"), incomingTitle.isEmpty() ? remoteDoc.value(QStringLiteral("title")).toString() : incomingTitle},
        {QStringLiteral("status"), incomingStatus},
        {QStringLiteral("version"), incomingVersion},
        {QStringLiteral("updatedAt"), QString::number(incomingUpdatedAt)}
    };

    state[QStringLiteral("documents")] = docs;
    return saveState(state);
}


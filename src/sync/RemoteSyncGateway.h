#pragma once

#include <QObject>
#include <QJsonObject>

#include "SyncTypes.h"

class IRemoteSyncGateway
{
public:
    virtual ~IRemoteSyncGateway() = default;
    virtual bool applyOperation(SyncOperationType operation, const QJsonObject& payload) = 0;
};

// MVP "remote" шлюз: сохраняет состояние документов в локальный JSON-файл.
// Это позволяет тестировать синхронизацию офлайн/онлайн без внешнего сервера.
class LocalRemoteSyncGateway final : public QObject, public IRemoteSyncGateway
{
    Q_OBJECT

public:
    explicit LocalRemoteSyncGateway(const QString& remoteStatePath, QObject* parent = nullptr);
    ~LocalRemoteSyncGateway() override = default;

    bool applyOperation(SyncOperationType operation, const QJsonObject& payload) override;

private:
    QString m_remoteStatePath;

    QJsonObject loadState() const;
    bool saveState(const QJsonObject& state) const;
};


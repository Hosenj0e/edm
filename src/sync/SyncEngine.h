#pragma once

#include <QObject>

#include <QNetworkInformation>

#include "SqliteSyncQueue.h"
#include "RemoteSyncGateway.h"

class SyncEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int pendingTasks READ pendingTasks NOTIFY pendingTasksChanged)

public:
    SyncEngine(SqliteSyncQueue& queue, IRemoteSyncGateway& remoteGateway, QObject* parent = nullptr);
    ~SyncEngine() override = default;

    int pendingTasks() const;

public slots:
    void flushNow();

signals:
    void pendingTasksChanged();

private:
    void setPendingTasksChanged();

private slots:
    void onOnlineStateChanged(bool online);

private:
    SqliteSyncQueue& m_queue;
    IRemoteSyncGateway& m_remoteGateway;
    bool m_isFlushing = false;

    static bool isNetworkOnline();
};


#include "SyncEngine.h"

#include <QDebug>

bool SyncEngine::isNetworkOnline()
{
    if (!QNetworkInformation::loadDefaultBackend())
        return true;
    QNetworkInformation* const ni = QNetworkInformation::instance();
    if (!ni)
        return true;
    return ni->reachability() == QNetworkInformation::Reachability::Online;
}

SyncEngine::SyncEngine(SqliteSyncQueue& queue, IRemoteSyncGateway& remoteGateway, QObject* parent)
    : QObject(parent)
    , m_queue(queue)
    , m_remoteGateway(remoteGateway)
{
    if (QNetworkInformation::loadDefaultBackend()) {
        if (QNetworkInformation* const ni = QNetworkInformation::instance()) {
            connect(ni, &QNetworkInformation::reachabilityChanged, this,
                    [this](QNetworkInformation::Reachability r) {
                        onOnlineStateChanged(r == QNetworkInformation::Reachability::Online);
                    });
        }
    }

    onOnlineStateChanged(isNetworkOnline());
}

int SyncEngine::pendingTasks() const
{
    return m_queue.pendingCount();
}

void SyncEngine::setPendingTasksChanged()
{
    emit pendingTasksChanged();
}

void SyncEngine::flushNow()
{
    if (m_isFlushing)
        return;
    if (!isNetworkOnline())
        return;

    m_isFlushing = true;

    // MVP: забираем задачи пачкой и применяем по очереди.
    const QVector<SyncTask> tasks = m_queue.dequeuePending(50);
    for (const auto& t : tasks) {
        const bool ok = m_remoteGateway.applyOperation(t.operation, t.payload);
        if (ok) {
            m_queue.markSucceeded(t.taskId);
        } else {
            m_queue.markFailed(t.taskId, QStringLiteral("Remote apply failed"));
        }
    }

    m_isFlushing = false;
    setPendingTasksChanged();
}

void SyncEngine::onOnlineStateChanged(bool online)
{
    if (!online)
        return;
    // Если сеть вернулась, синхронизируемся.
    flushNow();
}


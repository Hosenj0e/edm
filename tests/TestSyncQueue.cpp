#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

#include "sync/SqliteSyncQueue.h"
#include "sync/RemoteSyncGateway.h"

class TestSyncQueue final : public QObject
{
    Q_OBJECT

private slots:
    void testQueue_enqueueDequeueMarkSucceeded()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("sync.sqlite");
        SqliteSyncQueue queue(dbPath);

        QJsonObject payload;
        payload.insert(QStringLiteral("docId"), QStringLiteral("DOC-1"));
        payload.insert(QStringLiteral("title"), QStringLiteral("T"));
        payload.insert(QStringLiteral("status"), 0);
        payload.insert(QStringLiteral("version"), 1);
        payload.insert(QStringLiteral("updatedAt"), 1000);

        const QString taskId = queue.enqueue(SyncOperationType::Create, QStringLiteral("DOC-1"), payload);
        QVERIFY(!taskId.isEmpty());
        QCOMPARE(queue.pendingCount(), 1);

        const QVector<SyncTask> tasks = queue.dequeuePending(10);
        QCOMPARE(tasks.size(), 1);
        QCOMPARE(tasks[0].taskId, taskId);
        QCOMPARE(tasks[0].docId, QStringLiteral("DOC-1"));
        QCOMPARE(static_cast<int>(tasks[0].operation), static_cast<int>(SyncOperationType::Create));
        QVERIFY(tasks[0].payload.value(QStringLiteral("title")).toString() == QStringLiteral("T"));

        queue.markSucceeded(taskId);
        QCOMPARE(queue.pendingCount(), 0);
    }

    void testRemoteGateway_conflictResolutionByVersion()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString remotePath = tempDir.filePath("remote_state.json");
        LocalRemoteSyncGateway gateway(remotePath);

        QJsonObject createPayload;
        createPayload.insert(QStringLiteral("docId"), QStringLiteral("DOC-1"));
        createPayload.insert(QStringLiteral("title"), QStringLiteral("T"));
        createPayload.insert(QStringLiteral("status"), 0);
        createPayload.insert(QStringLiteral("version"), 1);
        createPayload.insert(QStringLiteral("updatedAt"), 1000);

        QVERIFY(gateway.applyOperation(SyncOperationType::Create, createPayload));

        QJsonObject approveOld = createPayload;
        approveOld.insert(QStringLiteral("status"), 2);
        approveOld.insert(QStringLiteral("version"), 1);
        approveOld.insert(QStringLiteral("updatedAt"), 900);
        QVERIFY(gateway.applyOperation(SyncOperationType::Approve, approveOld));

        QJsonObject approveNew = createPayload;
        approveNew.insert(QStringLiteral("status"), 2);
        approveNew.insert(QStringLiteral("version"), 2);
        approveNew.insert(QStringLiteral("updatedAt"), 1100);
        QVERIFY(gateway.applyOperation(SyncOperationType::Approve, approveNew));

        QFile f(remotePath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        QVERIFY(doc.isObject());

        const QJsonObject root = doc.object();
        const QJsonObject docs = root.value(QStringLiteral("documents")).toObject();
        const QJsonObject d1 = docs.value(QStringLiteral("DOC-1")).toObject();

        QCOMPARE(d1.value(QStringLiteral("version")).toInt(), 2);
        QCOMPARE(d1.value(QStringLiteral("status")).toInt(), 2);
    }
};

QTEST_MAIN(TestSyncQueue)
#include "TestSyncQueue.moc"


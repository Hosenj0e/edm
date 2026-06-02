#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QCryptographicHash>

#include "storage/SqliteDocumentRepository.h"
#include "domain/DocumentTypes.h"

class TestRepository final : public QObject
{
    Q_OBJECT

private slots:
    void testCreateApproveSign_updatesVersionAndStatus()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("documents.sqlite");
        SqliteDocumentRepository repo(dbPath);

        const QByteArray content = QByteArrayLiteral("hello");
        const QByteArray contentHash = QCryptographicHash::hash(content, QCryptographicHash::Sha256);

        const qint64 t1 = 1000;
        const qint64 t2 = 2000;
        const qint64 t3 = 3000;

        QVERIFY(repo.createDocument(QStringLiteral("DOC-1"),
                                    QStringLiteral("Test"),
                                    DocumentStatus::Draft,
                                    1,
                                    t1,
                                    t1,
                                    contentHash,
                                    QStringLiteral("local")));

        const QVector<DocumentSummary> afterCreate = repo.listDocumentSummaries();
        QVERIFY(afterCreate.size() >= 1);

        bool found = false;
        for (const auto& d : afterCreate) {
            if (d.id == QStringLiteral("DOC-1")) {
                found = true;
                QCOMPARE(d.status, DocumentStatus::Draft);
                QCOMPARE(d.version, 1);
            }
        }
        QVERIFY(found);

        QCOMPARE(repo.contentHashForDocument(QStringLiteral("DOC-1")), contentHash);

        QVERIFY(repo.approveDocument(QStringLiteral("DOC-1"), QStringLiteral("local"), t2));
        QCOMPARE(repo.versionForDocument(QStringLiteral("DOC-1")), 2);

        const QVector<DocumentSummary> afterApprove = repo.listDocumentSummaries();
        found = false;
        for (const auto& d : afterApprove) {
            if (d.id == QStringLiteral("DOC-1")) {
                found = true;
                QCOMPARE(d.status, DocumentStatus::Approved);
                QCOMPARE(d.version, 2);
            }
        }
        QVERIFY(found);

        const QByteArray signature(32, char(0xAB));
        QVERIFY(repo.signDocument(QStringLiteral("DOC-1"), QStringLiteral("local"), signature, t3, t3));
        QCOMPARE(repo.versionForDocument(QStringLiteral("DOC-1")), 3);

        const QVector<DocumentSummary> afterSign = repo.listDocumentSummaries();
        found = false;
        for (const auto& d : afterSign) {
            if (d.id == QStringLiteral("DOC-1")) {
                found = true;
                QCOMPARE(d.status, DocumentStatus::Signed);
                QCOMPARE(d.version, 3);
            }
        }
        QVERIFY(found);
    }
};

QTEST_MAIN(TestRepository)
#include "TestRepository.moc"


#include "DocumentService.h"

#include "../storage/SqliteDocumentRepository.h"
#include "../storage/EncryptedFileStorage.h"
#include "../security/SignatureProvider.h"
#include "../sync/SqliteSyncQueue.h"
#include "../sync/SyncTypes.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QUuid>
#include <QJsonObject>
#include <QFile>
#include <QVariantMap>
#include <QUrl>
#include <QDebug>
#include <utility>

using namespace std;

DocumentService::DocumentService(SqliteDocumentRepository& repository,
                                   EncryptedFileStorage& vault,
                                   ISignatureProvider& signatureProvider,
                                   SqliteSyncQueue& syncQueue,
                                   QString actorId,
                                   QObject* parent)
    : QObject(parent)
    , m_repository(repository)
    , m_vault(vault)
    , m_signatureProvider(signatureProvider)
    , m_syncQueue(syncQueue)
    , m_actorId(move(actorId))
{
}

QByteArray DocumentService::sha256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

qint64 DocumentService::nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString DocumentService::createDocument(const QString& title, const QByteArray& plaintextContent)
{
    const QString docId = QStringLiteral("DOC-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
    qDebug() << "[DocumentService] Creating document:" << docId << "title:" << title;

    const qint64 createdAt = nowMs();
    const qint64 updatedAt = createdAt;
    const int version = 1;
    const DocumentStatus status = DocumentStatus::Draft;

    const QByteArray contentHash = sha256(plaintextContent);
    qDebug() << "[DocumentService] Content hash:" << contentHash.toHex();

    if (!m_vault.writeDocumentContent(docId, plaintextContent)) {
        qDebug() << "[DocumentService] ERROR: Failed to write document content to vault!";
        return {};
    }
    qDebug() << "[DocumentService] Document content written to vault successfully";

    if (!m_repository.createDocument(docId, title, status, version, createdAt, updatedAt, contentHash, m_actorId)) {
        qDebug() << "[DocumentService] ERROR: Failed to create document in repository!";
        return {};
    }
    qDebug() << "[DocumentService] Document created in repository successfully";

    // Ставим задачу в офлайн-очередь.
    QJsonObject payload;
    payload.insert(QStringLiteral("docId"), docId);
    payload.insert(QStringLiteral("title"), title);
    payload.insert(QStringLiteral("status"), static_cast<int>(status));
    payload.insert(QStringLiteral("version"), version);
    payload.insert(QStringLiteral("updatedAt"), updatedAt);
    m_syncQueue.enqueue(SyncOperationType::Create, docId, payload);
    qDebug() << "[DocumentService] Sync task enqueued";

    qDebug() << "[DocumentService] SUCCESS: Document created:" << docId;
    return docId;
}

QVector<DocumentSummary> DocumentService::listDocuments()
{
    return m_repository.listDocumentSummaries();
}

bool DocumentService::approveDocument(const QString& docId)
{
    const qint64 updatedAt = nowMs();
    const bool ok = m_repository.approveDocument(docId, m_actorId, updatedAt);
    if (!ok)
        return false;

    const int version = m_repository.versionForDocument(docId);
    QJsonObject payload;
    payload.insert(QStringLiteral("docId"), docId);
    payload.insert(QStringLiteral("status"), static_cast<int>(DocumentStatus::Approved));
    payload.insert(QStringLiteral("version"), version);
    payload.insert(QStringLiteral("updatedAt"), updatedAt);
    m_syncQueue.enqueue(SyncOperationType::Approve, docId, payload);
    return true;
}

bool DocumentService::signDocument(const QString& docId)
{
    const QByteArray contentHash = m_repository.contentHashForDocument(docId);
    if (contentHash.isEmpty())
        return false;

    const qint64 signedAt = nowMs();
    const QByteArray signature = m_signatureProvider.sign(docId, contentHash);
    const bool ok = m_repository.signDocument(docId, m_actorId, signature, signedAt, signedAt);
    if (!ok)
        return false;

    const int version = m_repository.versionForDocument(docId);
    QJsonObject payload;
    payload.insert(QStringLiteral("docId"), docId);
    payload.insert(QStringLiteral("status"), static_cast<int>(DocumentStatus::Signed));
    payload.insert(QStringLiteral("version"), version);
    payload.insert(QStringLiteral("updatedAt"), signedAt);
    payload.insert(QStringLiteral("signature"), QString::fromUtf8(signature.toBase64()));
    m_syncQueue.enqueue(SyncOperationType::Sign, docId, payload);
    return true;
}

QString DocumentService::readDocumentPlainText(const QString& docId) const
{
    const QByteArray raw = m_vault.readDocumentContent(docId);
    if (raw.isEmpty())
        return {};
    return QString::fromUtf8(raw);
}

bool DocumentService::saveDraftPlainText(const QString& docId, const QString& plainText)
{
    if (m_repository.documentStatus(docId) != DocumentStatus::Draft)
        return false;

    const QByteArray utf8 = plainText.toUtf8();
    const QByteArray contentHash = sha256(utf8);
    const qint64 updatedAt = nowMs();

    if (!m_vault.writeDocumentContent(docId, utf8))
        return false;

    if (!m_repository.updateDraftContent(docId, contentHash, updatedAt))
        return false;

    const int version = m_repository.versionForDocument(docId);
    QJsonObject payload;
    payload.insert(QStringLiteral("docId"), docId);
    payload.insert(QStringLiteral("title"), QString()); // сервер может игнорировать
    payload.insert(QStringLiteral("status"), static_cast<int>(DocumentStatus::Draft));
    payload.insert(QStringLiteral("version"), version);
    payload.insert(QStringLiteral("updatedAt"), updatedAt);
    m_syncQueue.enqueue(SyncOperationType::Create, docId, payload);
    return true;
}

bool DocumentService::exportDecryptedToFile(const QString& docId, const QString& localFilePath) const
{
    const QByteArray raw = m_vault.readDocumentContent(docId);
    if (raw.isEmpty())
        return false;

    QString path = localFilePath;
    if (path.startsWith(QStringLiteral("file:///")))
        path = QUrl(localFilePath).toLocalFile();
    else if (path.startsWith(QStringLiteral("file://")))
        path = QUrl(localFilePath).toLocalFile();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(raw) == raw.size();
}

QVariantList DocumentService::listComments(const QString& docId) const
{
    return m_repository.listComments(docId);
}

bool DocumentService::addComment(const QString& docId, const QString& text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return false;
    return m_repository.addComment(docId, m_actorId, t, nowMs());
}


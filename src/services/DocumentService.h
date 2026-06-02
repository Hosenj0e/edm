#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QVariant>

#include "../domain/DocumentTypes.h"

class SqliteDocumentRepository;
class EncryptedFileStorage;
class ISignatureProvider;
class SqliteSyncQueue;

class DocumentService : public QObject
{
    Q_OBJECT

public:
    DocumentService(SqliteDocumentRepository& repository,
                     EncryptedFileStorage& vault,
                     ISignatureProvider& signatureProvider,
                     SqliteSyncQueue& syncQueue,
                     QString actorId,
                     QObject* parent = nullptr);
    ~DocumentService() override = default;

    QString createDocument(const QString& title, const QByteArray& plaintextContent);
    QVector<DocumentSummary> listDocuments();

    bool approveDocument(const QString& docId);
    bool signDocument(const QString& docId);

    QString readDocumentPlainText(const QString& docId) const;
    bool saveDraftPlainText(const QString& docId, const QString& plainText);
    bool exportDecryptedToFile(const QString& docId, const QString& localFilePath) const;

    QVariantList listComments(const QString& docId) const;
    bool addComment(const QString& docId, const QString& text);

private:
    static QByteArray sha256(const QByteArray& data);
    static qint64 nowMs();

    SqliteDocumentRepository& m_repository;
    EncryptedFileStorage& m_vault;
    ISignatureProvider& m_signatureProvider;
    SqliteSyncQueue& m_syncQueue;
    QString m_actorId;
};


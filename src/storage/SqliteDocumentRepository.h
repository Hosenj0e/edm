#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QVariantList>

#include "../domain/DocumentTypes.h"

class SqliteDocumentRepository : public QObject
{
    Q_OBJECT

public:
    explicit SqliteDocumentRepository(const QString& dbPath, QObject* parent = nullptr);
    ~SqliteDocumentRepository() override;

    QVector<DocumentSummary> listDocumentSummaries();

    bool createDocument(const QString& docId,
                         const QString& title,
                         DocumentStatus status,
                         int version,
                         qint64 createdAt,
                         qint64 updatedAt,
                         const QByteArray& contentHash,
                         const QString& initialApproverId);

    QByteArray contentHashForDocument(const QString& docId) const;

    int versionForDocument(const QString& docId) const;

    bool approveDocument(const QString& docId,
                          const QString& approverId,
                          qint64 updatedAt);

    bool signDocument(const QString& docId,
                       const QString& signerId,
                       const QByteArray& signature,
                       qint64 signedAt,
                       qint64 updatedAt);

    DocumentStatus documentStatus(const QString& docId) const;

    /// Обновляет содержимое только для черновика (status = Draft).
    bool updateDraftContent(const QString& docId, const QByteArray& contentHash, qint64 updatedAt);

    QVariantList listComments(const QString& docId) const;
    bool addComment(const QString& docId, const QString& authorId, const QString& body, qint64 createdAt);

private:
    bool ensureSchema();
    bool openOrMigrate();

    QString m_dbPath;
    QString m_connectionName;
};


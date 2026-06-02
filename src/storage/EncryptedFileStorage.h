#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>

class EncryptionService;

class EncryptedFileStorage : public QObject
{
    Q_OBJECT

public:
    EncryptedFileStorage(const EncryptionService& encryptionService, const QString& vaultDir, QObject* parent = nullptr);
    ~EncryptedFileStorage() override;

    bool writeDocumentContent(const QString& docId, const QByteArray& plaintext);
    QByteArray readDocumentContent(const QString& docId);

private:
    QByteArray aadForDoc(const QString& docId) const;
    QString m_vaultDir;
    const EncryptionService& m_encryptionService;
};


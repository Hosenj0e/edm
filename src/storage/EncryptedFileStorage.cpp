#include "EncryptedFileStorage.h"

#include "../security/EncryptionService.h"

#include <QDir>
#include <QFile>
#include <QDebug>

EncryptedFileStorage::EncryptedFileStorage(const EncryptionService& encryptionService, const QString& vaultDir, QObject* parent)
    : QObject(parent)
    , m_vaultDir(vaultDir)
    , m_encryptionService(encryptionService)
{
    qDebug() << "[EncryptedFileStorage] Initializing with vault dir:" << m_vaultDir;
    QDir().mkpath(m_vaultDir);
    qDebug() << "[EncryptedFileStorage] Vault directory created/verified";
}

EncryptedFileStorage::~EncryptedFileStorage() = default;

QByteArray EncryptedFileStorage::aadForDoc(const QString& docId) const
{
    return docId.toUtf8();
}

bool EncryptedFileStorage::writeDocumentContent(const QString& docId, const QByteArray& plaintext)
{
    const QString path = m_vaultDir + "/" + docId + ".enc";
    qDebug() << "[EncryptedFileStorage] Writing document:" << docId << "to path:" << path;
    qDebug() << "[EncryptedFileStorage] Vault dir:" << m_vaultDir;
    qDebug() << "[EncryptedFileStorage] Plaintext size:" << plaintext.size();
    
    QFile f(path);

    const QByteArray packet = m_encryptionService.encrypt(plaintext, aadForDoc(docId));
    if (packet.isEmpty()) {
        qDebug() << "[EncryptedFileStorage] ERROR: Encryption returned empty packet!";
        return false;
    }
    qDebug() << "[EncryptedFileStorage] Encrypted packet size:" << packet.size();

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "[EncryptedFileStorage] ERROR: Cannot open file for writing:" << path;
        qDebug() << "[EncryptedFileStorage] Error:" << f.errorString();
        return false;
    }

    const qint64 written = f.write(packet);
    f.flush();
    qDebug() << "[EncryptedFileStorage] Written bytes:" << written << "/ expected:" << packet.size();
    
    if (written != packet.size()) {
        qDebug() << "[EncryptedFileStorage] ERROR: Write size mismatch!";
        return false;
    }
    
    qDebug() << "[EncryptedFileStorage] SUCCESS: Document written successfully";
    return true;
}

QByteArray EncryptedFileStorage::readDocumentContent(const QString& docId)
{
    const QString path = m_vaultDir + "/" + docId + ".enc";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    const QByteArray packet = f.readAll();
    if (packet.isEmpty())
        return {};

    return m_encryptionService.decrypt(packet, aadForDoc(docId));
}


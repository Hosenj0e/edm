#pragma once

#include <QObject>
#include <QByteArray>

class KeyManager;

class EncryptionService : public QObject
{
    Q_OBJECT

public:
    explicit EncryptionService(const KeyManager& keyManager, QObject* parent = nullptr);
    ~EncryptionService() override;

    // Шифрует данные и добавляет аутентификацию (HMAC).
    // Формат пакета: [version(1)][iv(16)][ciphertext(n)][mac(32)]
    QByteArray encrypt(const QByteArray& plaintext, const QByteArray& aad = {}) const;

    // Возвращает пустой QByteArray при ошибке аутентификации/формата.
    QByteArray decrypt(const QByteArray& packet, const QByteArray& aad = {}) const;

private:
    QByteArray m_keyEnc;
    QByteArray m_keyMac;
};


#include "SignatureProvider.h"

#include "KeyManager.h"

#include <QCryptographicHash>

namespace {
QByteArray sha256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

QByteArray hmacSha256(const QByteArray& key, const QByteArray& msg)
{
    constexpr int HmacBlockSize = 64;

    QByteArray k = key;
    if (k.size() > HmacBlockSize)
        k = sha256(k);
    if (k.size() < HmacBlockSize)
        k.append(QByteArray(HmacBlockSize - k.size(), char(0)));

    QByteArray o_key_pad(HmacBlockSize, char(0));
    QByteArray i_key_pad(HmacBlockSize, char(0));

    for (int i = 0; i < HmacBlockSize; ++i) {
        const char c = k[i];
        o_key_pad[i] = c ^ 0x5c;
        i_key_pad[i] = c ^ 0x36;
    }

    const QByteArray inner = sha256(i_key_pad + msg);
    return sha256(o_key_pad + inner);
}
}

LocalSignatureProvider::LocalSignatureProvider(const KeyManager& keyManager, QObject* parent)
    : QObject(parent)
{
    const QByteArray master = keyManager.masterKey();
    m_keySign = sha256(master + QByteArrayLiteral("sign"));
}

QByteArray LocalSignatureProvider::sign(const QString& documentId, const QByteArray& documentHash)
{
    const QByteArray docIdBytes = documentId.toUtf8();
    QByteArray msg;
    msg.reserve(docIdBytes.size() + 1 + documentHash.size());
    msg.append(docIdBytes);
    msg.append(char(0));
    msg.append(documentHash);
    return hmacSha256(m_keySign, msg);
}


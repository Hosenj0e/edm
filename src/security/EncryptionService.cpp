#include "EncryptionService.h"

#include "KeyManager.h"

#include <algorithm>
#include <QCryptographicHash>
#include <QRandomGenerator>

using namespace std;

namespace {
constexpr quint8 PacketVersion = 1;
constexpr int IvSize = 16;
constexpr int MacSize = 32;
constexpr int HmacBlockSize = 64;

QByteArray sha256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

QByteArray hmacSha256(const QByteArray& key, const QByteArray& msg)
{
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

bool constantTimeEqual(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

QByteArray u32ToBigEndian(quint32 v)
{
    QByteArray out(4, char(0));
    out[0] = static_cast<char>((v >> 24) & 0xFF);
    out[1] = static_cast<char>((v >> 16) & 0xFF);
    out[2] = static_cast<char>((v >> 8) & 0xFF);
    out[3] = static_cast<char>(v & 0xFF);
    return out;
}
}

EncryptionService::EncryptionService(const KeyManager& keyManager, QObject* parent)
    : QObject(parent)
{
    const QByteArray master = keyManager.masterKey();
    m_keyEnc = sha256(master + QByteArrayLiteral("enc"));
    m_keyMac = sha256(master + QByteArrayLiteral("mac"));
}

EncryptionService::~EncryptionService() = default;

QByteArray EncryptionService::encrypt(const QByteArray& plaintext, const QByteArray& aad) const
{
    // Stream cipher: keystream = HMAC(keyEnc, iv||counter), XOR with plaintext.
    QByteArray iv(IvSize, char(0));
    for (int i = 0; i < IvSize; ++i)
        iv[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);

    QByteArray ciphertext;
    ciphertext.resize(plaintext.size());

    const int blockOutSize = MacSize; // HMAC-SHA256 output size
    quint32 counter = 0;

    for (int offset = 0; offset < plaintext.size(); offset += blockOutSize) {
        const int chunkSize = min<int>(blockOutSize, int(plaintext.size() - offset));

        const QByteArray prfInput = iv + u32ToBigEndian(counter);
        const QByteArray prfOut = hmacSha256(m_keyEnc, aad + prfInput);

        for (int i = 0; i < chunkSize; ++i)
            ciphertext[offset + i] = plaintext[offset + i] ^ prfOut[i];

        ++counter;
    }

    const QByteArray macInput = aad + iv + ciphertext;
    const QByteArray mac = hmacSha256(m_keyMac, macInput);

    QByteArray packet;
    packet.reserve(1 + IvSize + ciphertext.size() + MacSize);
    packet.append(static_cast<char>(PacketVersion));
    packet.append(iv);
    packet.append(ciphertext);
    packet.append(mac);
    return packet;
}

QByteArray EncryptionService::decrypt(const QByteArray& packet, const QByteArray& aad) const
{
    if (packet.size() < 1 + IvSize + MacSize)
        return {};

    const quint8 version = static_cast<quint8>(packet[0]);
    if (version != PacketVersion)
        return {};

    const QByteArray iv = packet.mid(1, IvSize);
    const QByteArray mac = packet.right(MacSize);
    const QByteArray ciphertext = packet.mid(1 + IvSize, packet.size() - (1 + IvSize + MacSize));

    const QByteArray macInput = aad + iv + ciphertext;
    const QByteArray expectedMac = hmacSha256(m_keyMac, macInput);
    if (!constantTimeEqual(mac, expectedMac))
        return {};

    QByteArray plaintext;
    plaintext.resize(ciphertext.size());

    const int blockOutSize = MacSize;
    quint32 counter = 0;
    for (int offset = 0; offset < ciphertext.size(); offset += blockOutSize) {
        const int chunkSize = min<int>(blockOutSize, int(ciphertext.size() - offset));
        const QByteArray prfInput = iv + u32ToBigEndian(counter);
        const QByteArray prfOut = hmacSha256(m_keyEnc, aad + prfInput);

        for (int i = 0; i < chunkSize; ++i)
            plaintext[offset + i] = ciphertext[offset + i] ^ prfOut[i];

        ++counter;
    }

    return plaintext;
}


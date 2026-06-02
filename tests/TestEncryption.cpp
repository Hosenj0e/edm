#include <QtTest/QtTest>

#include "security/KeyManager.h"
#include "security/EncryptionService.h"

class TestEncryption final : public QObject
{
    Q_OBJECT

public:
    TestEncryption()
    {
        QCoreApplication::setOrganizationName("offline-edm-test");
        QCoreApplication::setApplicationName("offline-edm-test");
    }

private slots:
    void testEncryptDecrypt_roundtrip()
    {
        KeyManager km;
        EncryptionService enc(km);

        const QByteArray plaintext = QByteArrayLiteral("Hello secret data");
        const QByteArray aad = QByteArrayLiteral("DOC-TEST1");

        const QByteArray packet = enc.encrypt(plaintext, aad);
        QVERIFY(!packet.isEmpty());

        const QByteArray decrypted = enc.decrypt(packet, aad);
        QCOMPARE(decrypted, plaintext);
    }

    void testEncryptDecrypt_tamperDetects()
    {
        KeyManager km;
        EncryptionService enc(km);

        const QByteArray plaintext = QByteArrayLiteral("Hello secret data");
        const QByteArray aad = QByteArrayLiteral("DOC-TEST1");

        QByteArray packet = enc.encrypt(plaintext, aad);
        QVERIFY(packet.size() > 40); // version + iv + mac (roughly)

        packet[packet.size() / 2] = packet[packet.size() / 2] ^ 0x01;

        const QByteArray decrypted = enc.decrypt(packet, aad);
        QVERIFY(decrypted.isEmpty());
    }

    void testDecrypt_wrongAadFails()
    {
        KeyManager km;
        EncryptionService enc(km);

        const QByteArray plaintext = QByteArrayLiteral("Hello secret data");
        const QByteArray aad = QByteArrayLiteral("DOC-TEST1");
        const QByteArray wrongAad = QByteArrayLiteral("DOC-OTHER");

        const QByteArray packet = enc.encrypt(plaintext, aad);
        QVERIFY(!packet.isEmpty());

        const QByteArray decrypted = enc.decrypt(packet, wrongAad);
        QVERIFY(decrypted.isEmpty());
    }
};

QTEST_MAIN(TestEncryption)
#include "TestEncryption.moc"


#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class KeyManager;

class ISignatureProvider
{
public:
    virtual ~ISignatureProvider() = default;
    virtual QByteArray sign(const QString& documentId, const QByteArray& documentHash) = 0;
};

class LocalSignatureProvider final : public QObject, public ISignatureProvider
{
    Q_OBJECT

public:
    LocalSignatureProvider(const KeyManager& keyManager, QObject* parent = nullptr);
    ~LocalSignatureProvider() override = default;

    QByteArray sign(const QString& documentId, const QByteArray& documentHash) override;

private:
    QByteArray m_keySign;
};


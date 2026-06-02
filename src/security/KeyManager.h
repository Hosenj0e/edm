#pragma once

#include <QObject>
#include <QByteArray>

class KeyManager : public QObject
{
    Q_OBJECT

public:
    /// Если userDataDirectory пустой — используется каталог по умолчанию (совместимость с тестами).
    explicit KeyManager(const QString& userDataDirectory = QString(), QObject* parent = nullptr);
    ~KeyManager() override;

    QByteArray masterKey() const;

private:
    QByteArray m_masterKey;
};


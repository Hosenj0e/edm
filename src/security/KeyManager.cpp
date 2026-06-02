#include "KeyManager.h"

#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QStandardPaths>

namespace {
constexpr int MasterKeySize = 32;

QByteArray generateRandomBytes(int size)
{
    QByteArray out;
    out.resize(size);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < size; ++i)
        out[i] = static_cast<char>(rng->generate() & 0xFF);
    return out;
}
}

KeyManager::KeyManager(const QString& userDataDirectory, QObject* parent)
    : QObject(parent)
{
    QString dataDir = userDataDirectory;
    if (dataDir.isEmpty()) {
        dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/offline-edm");
    }
    QDir().mkpath(dataDir);

    const QString keyPath = dataDir + QStringLiteral("/offline-edm_master.key");
    QFile f(keyPath);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        const QByteArray loaded = f.readAll();
        if (loaded.size() == MasterKeySize) {
            m_masterKey = loaded;
            return;
        }
    }

    m_masterKey = generateRandomBytes(MasterKeySize);

    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(m_masterKey);
        f.flush();
    }
}

KeyManager::~KeyManager() = default;

QByteArray KeyManager::masterKey() const
{
    return m_masterKey;
}


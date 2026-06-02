#include "PortalSessionStore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>

bool PortalSessionStore::save(const QString& filePath, const QList<QNetworkCookie>& cookies)
{
    QJsonArray arr;
    for (const QNetworkCookie& c : cookies) {
        if (c.expirationDate().isValid() && c.expirationDate() < QDateTime::currentDateTimeUtc())
            continue;

        QJsonObject o;
        o.insert(QStringLiteral("name"), QString::fromUtf8(c.name()));
        o.insert(QStringLiteral("value"), QString::fromUtf8(c.value()));
        o.insert(QStringLiteral("domain"), c.domain());
        o.insert(QStringLiteral("path"), c.path());
        o.insert(QStringLiteral("secure"), c.isSecure());
        o.insert(QStringLiteral("httpOnly"), c.isHttpOnly());
        if (c.expirationDate().isValid())
            o.insert(QStringLiteral("expiration"), c.expirationDate().toString(Qt::ISODate));
        arr.append(o);
    }

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    return true;
}

bool PortalSessionStore::load(const QString& filePath, QList<QNetworkCookie>* outCookies)
{
    if (!outCookies)
        return false;
    outCookies->clear();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return false;

    for (const QJsonValue& v : doc.array()) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        const QByteArray name = o.value(QStringLiteral("name")).toString().toUtf8();
        const QByteArray value = o.value(QStringLiteral("value")).toString().toUtf8();
        if (name.isEmpty())
            continue;

        QNetworkCookie cookie(name, value);
        if (o.contains(QStringLiteral("domain")))
            cookie.setDomain(o.value(QStringLiteral("domain")).toString());
        if (o.contains(QStringLiteral("path")))
            cookie.setPath(o.value(QStringLiteral("path")).toString());
        cookie.setSecure(o.value(QStringLiteral("secure")).toBool(false));
        cookie.setHttpOnly(o.value(QStringLiteral("httpOnly")).toBool(false));
        const QString exp = o.value(QStringLiteral("expiration")).toString();
        if (!exp.isEmpty())
            cookie.setExpirationDate(QDateTime::fromString(exp, Qt::ISODate));
        outCookies->append(cookie);
    }
    return !outCookies->isEmpty();
}

bool PortalSessionStore::exists(const QString& filePath)
{
    return QFile::exists(filePath);
}

bool PortalSessionStore::remove(const QString& filePath)
{
    if (!QFile::exists(filePath))
        return true;
    return QFile::remove(filePath);
}

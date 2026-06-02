#pragma once

#include <QList>
#include <QString>

class QNetworkCookie;

class PortalSessionStore
{
public:
    static bool save(const QString& filePath, const QList<QNetworkCookie>& cookies);
    static bool load(const QString& filePath, QList<QNetworkCookie>* outCookies);
    static bool exists(const QString& filePath);
    static bool remove(const QString& filePath);
};

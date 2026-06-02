#pragma once

#include "PortalTypes.h"

class NovsuHtmlParser
{
public:
    static QString decodeEntities(const QString& html);
    static QString stripTags(const QString& html);

    static PortalLoginForm parseLoginForm(const QString& html, const QUrl& pageUrl);
    static QList<QUrl> extractGroupPageUrls(const QString& html, const QUrl& baseUrl);
    static bool parseGroupPage(const QString& html, const QUrl& pageUrl, PortalGroupDetail* out);
};

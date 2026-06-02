#include "NovsuHtmlParser.h"

#include <QRegularExpression>
#include <QSet>
#include <QUrl>

using namespace std;

namespace {

QString normalizeWs(const QString& s)
{
    QString t = s;
    t.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return t.trimmed();
}

QString mapPortalStatus(const QString& raw)
{
    const QString t = raw.trimmed();
    if (t.compare(QStringLiteral("СТ"), Qt::CaseInsensitive) == 0
        || t.contains(QStringLiteral("студ"), Qt::CaseInsensitive))
        return QStringLiteral("обучается");
    if (t.isEmpty())
        return QStringLiteral("обучается");
    return t;
}

QString fieldFromHtml(const QString& html, const QString& label)
{
    // Сначала пробуем извлечь из plain text (более надежно для списков <li>)
    const QString plain = NovsuHtmlParser::stripTags(html);
    QRegularExpression plainRe(QStringLiteral("%1\\s*:\\s*([^\\n•]+?)(?:\\s*(?:<|•|\\n|$))").arg(QRegularExpression::escape(label)),
                               QRegularExpression::CaseInsensitiveOption);
    const auto pm = plainRe.match(plain);
    if (pm.hasMatch()) {
        QString result = pm.captured(1).trimmed();
        // Убираем всё после следующего поля (если есть)
        int nextFieldPos = result.indexOf(QRegularExpression(QStringLiteral("\\s+[А-Яа-яA-Za-z]+\\s*:")));
        if (nextFieldPos > 0) {
            result = result.left(nextFieldPos).trimmed();
        }
        return normalizeWs(result);
    }

    // Fallback: старый метод через HTML
    const QString pattern =
        QStringLiteral("(?:<[^>]+>|[•\\-])?\\s*%1\\s*:?\\s*</[^>]+>\\s*([^<]+)").arg(QRegularExpression::escape(label));
    QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const auto m = re.match(html);
    if (m.hasMatch())
        return normalizeWs(NovsuHtmlParser::decodeEntities(m.captured(1)));

    return {};
}

} // namespace

QString NovsuHtmlParser::decodeEntities(const QString& html)
{
    QString s = html;
    s.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    QRegularExpression numEnt(QStringLiteral("&#(\\d+);"));
    auto entIt = numEnt.globalMatch(s);
    while (entIt.hasNext()) {
        const auto m = entIt.next();
        s.replace(m.captured(0), QChar(m.captured(1).toInt()));
        entIt = numEnt.globalMatch(s);
    }
    return s;
}

QString NovsuHtmlParser::stripTags(const QString& html)
{
    QString s = decodeEntities(html);
    const QRegularExpression::PatternOptions dotCi =
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption;
    s.remove(QRegularExpression(QStringLiteral("<script[^>]*>.*?</script>"), dotCi));
    s.remove(QRegularExpression(QStringLiteral("<style[^>]*>.*?</style>"), dotCi));
    s.replace(QRegularExpression(QString("<br\\s*/?>")), QStringLiteral("\n"));
    s.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QStringLiteral(" "));
    return normalizeWs(s);
}

PortalLoginForm NovsuHtmlParser::parseLoginForm(const QString& html, const QUrl& pageUrl)
{
    PortalLoginForm form;
    QRegularExpression formRe(
        QStringLiteral("<form\\b([^>]*)>(.*?)</form>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto it = formRe.globalMatch(html);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString attrs = m.captured(1);
        const QString body = m.captured(2);
        if (!body.contains(QStringLiteral("type=\"password\""), Qt::CaseInsensitive)
            && !body.contains(QStringLiteral("type='password'"), Qt::CaseInsensitive))
            continue;

        QString action;
        QRegularExpression actionRe(QStringLiteral("action\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
        const auto am = actionRe.match(attrs);
        if (am.hasMatch())
            action = am.captured(1);

        QString method = QStringLiteral("POST");
        QRegularExpression methodRe(QStringLiteral("method\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
        const auto mm = methodRe.match(attrs);
        if (mm.hasMatch())
            method = mm.captured(1).toUpper();

        form.fields.clear();
        QRegularExpression inputRe(
            QStringLiteral("<input\\b([^>]*)/?>"),
            QRegularExpression::CaseInsensitiveOption);
        auto inIt = inputRe.globalMatch(body);
        while (inIt.hasNext()) {
            const auto im = inIt.next();
            const QString inAttrs = im.captured(1);
            QString name;
            QString value;
            QString type = QStringLiteral("text");

            QRegularExpression nameRe(QStringLiteral("name\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
            const auto nm = nameRe.match(inAttrs);
            if (nm.hasMatch())
                name = nm.captured(1);

            QRegularExpression valRe(QStringLiteral("value\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
            const auto vm = valRe.match(inAttrs);
            if (vm.hasMatch())
                value = decodeEntities(vm.captured(1));

            QRegularExpression typeRe(QStringLiteral("type\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
            const auto tm = typeRe.match(inAttrs);
            if (tm.hasMatch())
                type = tm.captured(1).toLower();

            if (name.isEmpty() || type == QStringLiteral("submit") || type == QStringLiteral("button")
                || type == QStringLiteral("image"))
                continue;
            form.fields.insert(name, value);
        }

        form.action = pageUrl.resolved(QUrl(action.isEmpty() ? QString() : action));
        form.method = method;
        form.valid = !form.fields.isEmpty();
        return form;
    }
    return form;
}

QList<QUrl> NovsuHtmlParser::extractGroupPageUrls(const QString& html, const QUrl& baseUrl)
{
    QList<QUrl> urls;
    QSet<QString> seen;

    auto addUrl = [&](const QString& href) {
        if (href.isEmpty() || href.startsWith('#') || href.startsWith(QStringLiteral("javascript:"), Qt::CaseInsensitive))
            return;
        const QUrl u = baseUrl.resolved(href);
        if (!u.isValid() || (u.scheme() != QStringLiteral("http") && u.scheme() != QStringLiteral("https")))
            return;
        const QString key = u.toString(QUrl::RemoveFragment);
        if (seen.contains(key))
            return;
        seen.insert(key);
        urls.append(u);
    };

    QRegularExpression linkRe(
        QStringLiteral("<a\\b[^>]*href\\s*=\\s*[\"']([^\"']+)[\"'][^>]*>(.*?)</a>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto it = linkRe.globalMatch(html);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString href = decodeEntities(m.captured(1));
        const QString text = normalizeWs(stripTags(m.captured(2)));
        const bool numericGroup = QRegularExpression(QStringLiteral("^\\d{3,5}$")).match(text).hasMatch();
        const bool groupHref = href.contains(QStringLiteral("group"), Qt::CaseInsensitive)
            || href.contains(QStringLiteral("Group"), Qt::CaseInsensitive)
            || href.contains(QStringLiteral("studgr"), Qt::CaseInsensitive)
            || href.contains(QStringLiteral("name="), Qt::CaseInsensitive)
            || href.contains(QStringLiteral("ViewGroup"), Qt::CaseInsensitive)
            || href.contains(QStringLiteral("групп"), Qt::CaseInsensitive);
        if (numericGroup || groupHref)
            addUrl(href);
    }

    if (urls.isEmpty()) {
        QRegularExpression hrefOnly(
            QStringLiteral("href\\s*=\\s*[\"']([^\"']*(?:group|Group|ViewGroup|studgr)[^\"']*)[\"']"),
            QRegularExpression::CaseInsensitiveOption);
        auto hIt = hrefOnly.globalMatch(html);
        while (hIt.hasNext())
            addUrl(decodeEntities(hIt.next().captured(1)));
    }

    return urls;
}

bool NovsuHtmlParser::parseGroupPage(const QString& html, const QUrl& pageUrl, PortalGroupDetail* out)
{
    if (!out)
        return false;

    PortalGroupDetail g;
    g.sourceUrl = pageUrl.toString(QUrl::RemoveFragment);

    g.number = fieldFromHtml(html, QStringLiteral("Группа"));
    if (g.number.isEmpty()) {
        QRegularExpression hRe(QStringLiteral("Группа\\s+(\\d{3,5})"), QRegularExpression::CaseInsensitiveOption);
        const auto hm = hRe.match(stripTags(html));
        if (hm.hasMatch())
            g.number = hm.captured(1);
    }
    
    // Очищаем номер группы - оставляем только цифры
    if (!g.number.isEmpty()) {
        QRegularExpression digitsOnly(QStringLiteral("(\\d{3,5})"));
        const auto dm = digitsOnly.match(g.number);
        if (dm.hasMatch()) {
            g.number = dm.captured(1);
        }
    }

    qDebug() << "Parsing group page. Found group number:" << g.number;

    g.admissionYear = fieldFromHtml(html, QStringLiteral("Год поступления"));
    g.course = fieldFromHtml(html, QStringLiteral("Курс"));
    g.specialty = fieldFromHtml(html, QStringLiteral("Направление (специальность)"));
    if (g.specialty.isEmpty())
        g.specialty = fieldFromHtml(html, QStringLiteral("Направление"));
    g.profile = fieldFromHtml(html, QStringLiteral("Профиль"));
    g.institute = fieldFromHtml(html, QStringLiteral("Институт"));
    g.studyForm = fieldFromHtml(html, QStringLiteral("Форма обучения"));

    // Очищаем course - оставляем только первую цифру
    if (!g.course.isEmpty()) {
        QRegularExpression courseDigit(QStringLiteral("(\\d)"));
        const auto cm = courseDigit.match(g.course);
        if (cm.hasMatch()) {
            g.course = cm.captured(1);
        }
    }
    
    qDebug() << "  admissionYear:" << g.admissionYear;
    qDebug() << "  course:" << g.course;
    qDebug() << "  specialty:" << g.specialty;
    qDebug() << "  institute:" << g.institute;
    qDebug() << "  studyForm:" << g.studyForm;

    QRegularExpression rowRe(
        QStringLiteral("<tr\\b[^>]*>\\s*"
                       "<td[^>]*>\\s*(\\d+)\\s*</td>\\s*"
                       "<td[^>]*>\\s*(?:<a\\b[^>]*>)?\\s*([^<]+?)\\s*(?:</a>)?\\s*</td>\\s*"
                       "<td[^>]*>\\s*([^<]+?)\\s*</td>\\s*"
                       "</tr>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto rit = rowRe.globalMatch(html);
    int studentCount = 0;
    while (rit.hasNext()) {
        const auto m = rit.next();
        PortalStudentRow row;
        row.seq = m.captured(1).trimmed();
        row.fio = normalizeWs(decodeEntities(m.captured(2)));
        row.status = mapPortalStatus(decodeEntities(m.captured(3)));
        if (!row.fio.isEmpty()) {
            g.students.append(row);
            studentCount++;
        }
    }

    qDebug() << "  students found:" << studentCount;

    if (g.number.isEmpty() && g.students.isEmpty()) {
        qDebug() << "  Parse failed: no group number and no students";
        return false;
    }

    if (g.number.isEmpty())
        g.number = pageUrl.path().section(QLatin1Char('/'), -1).trimmed();

    qDebug() << "  Parse successful. Final group number:" << g.number;
    const bool result = !g.number.isEmpty();  // ВАЖНО: проверяем ДО move!
    qDebug() << "  parseGroupPage returning:" << result;
    *out = move(g);  // Перемещаем ПОСЛЕ проверки
    return result;
}

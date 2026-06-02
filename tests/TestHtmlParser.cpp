#include <QtTest/QtTest>
#include "portal/NovsuHtmlParser.h"
#include "portal/PortalTypes.h"

class TestHtmlParser final : public QObject
{
    Q_OBJECT

private slots:
    void testParseGroupPage_validHtml()
    {
        // Минимальный HTML страницы группы
        const QString html = R"(
            <html>
            <body>
                <div class="group-info">
                    <span class="field-label">Номер группы:</span>
                    <span class="field-value">2991</span>
                </div>
                <div class="group-info">
                    <span class="field-label">Год поступления:</span>
                    <span class="field-value">2022</span>
                </div>
                <div class="group-info">
                    <span class="field-label">Курс:</span>
                    <span class="field-value">4</span>
                </div>
                <table class="students">
                    <tr>
                        <td>1</td>
                        <td>Иванов Иван Иванович</td>
                        <td>обучается</td>
                    </tr>
                    <tr>
                        <td>2</td>
                        <td>Петров Петр Петрович</td>
                        <td>обучается</td>
                    </tr>
                </table>
            </body>
            </html>
        )";

        PortalGroupDetail detail;
        const QUrl baseUrl("https://portal.novsu.ru/group/2991");
        
        const bool result = NovsuHtmlParser::parseGroupPage(html, baseUrl, &detail);
        
        QVERIFY(result);
        QCOMPARE(detail.number, QStringLiteral("2991"));
        QCOMPARE(detail.admissionYear, QStringLiteral("2022"));
        QCOMPARE(detail.course, QStringLiteral("4"));
        QVERIFY(detail.students.size() >= 2);
    }

    void testParseGroupPage_emptyHtml()
    {
        const QString html = "<html><body></body></html>";
        PortalGroupDetail detail;
        const QUrl baseUrl("https://portal.novsu.ru/");
        
        const bool result = NovsuHtmlParser::parseGroupPage(html, baseUrl, &detail);
        
        QVERIFY(!result);
    }

    void testParseLoginForm_validForm()
    {
        const QString html = R"(
            <html>
            <body>
                <form action="/login" method="POST">
                    <input type="text" name="username" />
                    <input type="password" name="password" />
                    <input type="submit" value="Login" />
                </form>
            </body>
            </html>
        )";

        const QUrl baseUrl("https://portal.novsu.ru/");
        const PortalLoginForm form = NovsuHtmlParser::parseLoginForm(html, baseUrl);
        
        QVERIFY(form.valid);
        QCOMPARE(form.action, QUrl("https://portal.novsu.ru/login"));
        QVERIFY(form.fields.contains("username"));
        QVERIFY(form.fields.contains("password"));
    }

    void testExtractGroupPageUrls_validIndex()
    {
        const QString html = R"(
            <html>
            <body>
                <a href="/group/2991">Группа 2991</a>
                <a href="/group/2992">Группа 2992</a>
                <a href="/other/page">Другая ссылка</a>
            </body>
            </html>
        )";

        const QUrl baseUrl("https://portal.novsu.ru/");
        const QList<QUrl> urls = NovsuHtmlParser::extractGroupPageUrls(html, baseUrl);
        
        QVERIFY(urls.size() >= 2);
        
        bool found2991 = false;
        bool found2992 = false;
        for (const QUrl& url : urls) {
            if (url.toString().contains("2991")) found2991 = true;
            if (url.toString().contains("2992")) found2992 = true;
        }
        
        QVERIFY(found2991);
        QVERIFY(found2992);
    }

    void testFieldFromHtml_extractsValue()
    {
        const QString html = R"(
            <html>
            <body>
                <div class="group-info">
                    <span class="field-label">Номер группы:</span>
                    <span class="field-value">2991</span>
                </div>
                <table class="students">
                    <tr>
                        <td>1</td>
                        <td>Тестовый Студент</td>
                        <td>обучается</td>
                    </tr>
                </table>
            </body>
            </html>
        )";

        // Этот тест проверяет внутреннюю функцию через публичный API
        PortalGroupDetail detail;
        const QUrl baseUrl("https://portal.novsu.ru/group/2991");
        
        QVERIFY(NovsuHtmlParser::parseGroupPage(html, baseUrl, &detail));
        QCOMPARE(detail.number, QStringLiteral("2991"));
    }
};

QTEST_MAIN(TestHtmlParser)
#include "TestHtmlParser.moc"

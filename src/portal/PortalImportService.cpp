#include "PortalImportService.h"

#include "NovsuHtmlParser.h"
#include "PortalSessionStore.h"
#include "PortalTypes.h"

#include "../storage/AcademicRepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

namespace {

QNetworkRequest portalRequest(const QUrl& url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("offline-edm/1.0 (NovSU portal sync)"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

QString findPasswordFieldName(const PortalLoginForm& form)
{
    for (auto it = form.fields.constBegin(); it != form.fields.constEnd(); ++it) {
        const QString n = it.key().toLower();
        if (n.contains(QStringLiteral("pass")) || n.contains(QStringLiteral("pwd")))
            return it.key();
    }
    return QStringLiteral("password");
}

QString findLoginFieldName(const PortalLoginForm& form)
{
    for (auto it = form.fields.constBegin(); it != form.fields.constEnd(); ++it) {
        const QString n = it.key().toLower();
        if (n.contains(QStringLiteral("login")) || n.contains(QStringLiteral("user"))
            || n == QStringLiteral("email"))
            return it.key();
    }
    return QStringLiteral("login");
}

} // namespace

PortalImportService::PortalImportService(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_jar(new QNetworkCookieJar(this))
{
    m_nam->setCookieJar(m_jar);
    connect(m_nam, &QNetworkAccessManager::finished, this, &PortalImportService::onReplyFinished);
}

PortalImportService::~PortalImportService() = default;

void PortalImportService::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(m_busy);
}

void PortalImportService::setProgress(const QString& message, int percent)
{
    emit progressChanged(message, qBound(0, percent, 100));
}

void PortalImportService::startSync(AcademicRepository* repo, const QString& login, const QString& password,
                                    const QString& groupsIndexUrl, const QString& sessionStorePath)
{
    if (m_busy) {
        emit failed(tr("Синхронизация уже выполняется."));
        return;
    }
    if (!repo) {
        emit failed(tr("Сессия не открыта."));
        return;
    }
    if (groupsIndexUrl.trimmed().isEmpty()) {
        emit failed(tr("Укажите URL страницы со списком ваших групп (из браузера после входа)."));
        return;
    }

    const bool hasCredentials = !login.trimmed().isEmpty() && !password.isEmpty();
    m_sessionStorePath = sessionStorePath;
    const bool hasSession = !m_sessionStorePath.isEmpty() && PortalSessionStore::exists(m_sessionStorePath);

    if (!hasCredentials && !hasSession) {
        emit failed(tr("Войдите на портал один раз (логин и пароль) с галочкой «Запомнить сессию», "
                        "или используйте импорт HTML из папки."));
        return;
    }

    m_repo = repo;
    m_login = login.trimmed();
    m_password = password;
    m_indexUrl = groupsIndexUrl.trimmed();
    m_groupUrls.clear();
    m_groupIndex = 0;
    m_groupsImported = 0;
    m_studentsAdded = 0;

    setBusy(true);

    if (!hasCredentials && hasSession && loadSessionCookies()) {
        m_step = Step::CheckingSession;
        setProgress(tr("Проверка сохранённой сессии…"), 10);
        beginRequest(QUrl(m_indexUrl));
        return;
    }

    if (!hasCredentials) {
        fail(tr("Сохранённая сессия портала недействительна. Введите логин и пароль снова."));
        return;
    }

    m_step = Step::LoadingHome;
    setProgress(tr("Подключение к порталу…"), 2);
    beginRequest(m_baseUrl);
}

bool PortalImportService::loadSessionCookies()
{
    if (m_sessionStorePath.isEmpty())
        return false;
    QList<QNetworkCookie> cookies;
    if (!PortalSessionStore::load(m_sessionStorePath, &cookies))
        return false;
    for (const QNetworkCookie& c : cookies)
        m_jar->insertCookie(c);
    return true;
}

void PortalImportService::saveSessionCookies()
{
    if (m_sessionStorePath.isEmpty())
        return;

    QList<QNetworkCookie> cookies = m_jar->cookiesForUrl(m_baseUrl);
    const QList<QNetworkCookie> indexCookies = m_jar->cookiesForUrl(QUrl(m_indexUrl));
    for (const QNetworkCookie& c : indexCookies) {
        bool dup = false;
        for (const QNetworkCookie& existing : cookies) {
            if (existing.name() == c.name() && existing.domain() == c.domain()) {
                dup = true;
                break;
            }
        }
        if (!dup)
            cookies.append(c);
    }
    if (!cookies.isEmpty())
        PortalSessionStore::save(m_sessionStorePath, cookies);
}

bool PortalImportService::pageLooksLikeLogin(const QString& html) const
{
    const PortalLoginForm form = NovsuHtmlParser::parseLoginForm(html, m_baseUrl);
    return form.valid && html.contains(QStringLiteral("password"), Qt::CaseInsensitive);
}

void PortalImportService::beginRequest(const QUrl& url)
{
    qDebug() << "=== beginRequest CALLED ===";
    qDebug() << "URL:" << url.toString();
    qDebug() << "m_nam:" << m_nam;
    qDebug() << "Sending GET request...";
    m_nam->get(portalRequest(url));
    qDebug() << "=== beginRequest FINISHED ===";
}

void PortalImportService::postLogin(const PortalLoginForm& form)
{
    QUrlQuery query;
    for (auto it = form.fields.constBegin(); it != form.fields.constEnd(); ++it)
        query.addQueryItem(it.key(), it.value());

    const QString loginField = findLoginFieldName(form);
    const QString passField = findPasswordFieldName(form);
    query.removeAllQueryItems(loginField);
    query.removeAllQueryItems(passField);
    query.addQueryItem(loginField, m_login);
    query.addQueryItem(passField, m_password);

    QNetworkRequest req(portalRequest(form.action));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    m_nam->post(req, query.toString(QUrl::FullyEncoded).toUtf8());
}

void PortalImportService::fail(const QString& message)
{
    m_step = Step::Idle;
    m_repo = nullptr;
    setBusy(false);
    emit failed(message);
}

void PortalImportService::finishSuccess()
{
    saveSessionCookies();
    const int groups = m_groupsImported;
    const int students = m_studentsAdded;
    m_step = Step::Idle;
    m_repo = nullptr;
    setBusy(false);
    setProgress(tr("Готово"), 100);
    emit finished(groups, students);
}

void PortalImportService::handleHomePage(const QByteArray& body, const QUrl& url)
{
    const QString html = QString::fromUtf8(body);
    const PortalLoginForm form = NovsuHtmlParser::parseLoginForm(html, url);
    if (!form.valid) {
        fail(tr("На главной странице портала не найдена форма входа."));
        return;
    }
    m_step = Step::PostingLogin;
    setProgress(tr("Вход на портал…"), 8);
    postLogin(form);
}

void PortalImportService::handleLoginResult()
{
    saveSessionCookies();
    m_step = Step::LoadingIndex;
    setProgress(tr("Загрузка списка групп…"), 15);
    beginRequest(QUrl(m_indexUrl));
}

void PortalImportService::handleIndexPage(const QByteArray& body, const QUrl& url)
{
    const QString html = QString::fromUtf8(body);

    if (pageLooksLikeLogin(html)) {
        PortalSessionStore::remove(m_sessionStorePath);
        fail(tr("Сессия портала истекла. Введите логин и пароль ещё раз (с «Запомнить сессию»)."));
        return;
    }

    saveSessionCookies();

    m_groupUrls = NovsuHtmlParser::extractGroupPageUrls(html, url);

    if (m_groupUrls.isEmpty()) {
        PortalGroupDetail single;
        if (NovsuHtmlParser::parseGroupPage(html, url, &single)) {
            m_groupUrls.append(url);
        }
    }

    if (m_groupUrls.isEmpty()) {
        fail(tr("На указанной странице не найдены ссылки на группы. "
                "Откройте в браузере страницу со списком групп и скопируйте её адрес."));
        return;
    }

    m_groupIndex = 0;
    m_step = Step::LoadingGroup;
    setProgress(tr("Найдено групп: %1").arg(m_groupUrls.size()), 20);
    scheduleNextGroup();
}

void PortalImportService::scheduleNextGroup()
{
    if (!m_repo) {
        fail(tr("Сессия прервана."));
        return;
    }
    if (m_groupIndex >= m_groupUrls.size()) {
        finishSuccess();
        return;
    }

    const int pct = 20 + (m_groupIndex * 75) / qMax(1, m_groupUrls.size());
    setProgress(tr("Группа %1 из %2…").arg(m_groupIndex + 1).arg(m_groupUrls.size()), pct);
    beginRequest(m_groupUrls.at(m_groupIndex));
}

bool PortalImportService::applyGroupDetail(const PortalGroupDetail& detail)
{
    if (!m_repo || detail.number.trimmed().isEmpty()) {
        qDebug() << "applyGroupDetail: repo or number empty";
        return false;
    }

    qDebug() << "Applying group:" << detail.number << "admYear:" << detail.admissionYear 
             << "course:" << detail.course << "students:" << detail.students.size();

    // Фильтрация: пропускаем неактивные группы, если включен фильтр
    if (m_filterActiveOnly) {
        if (!detail.isActiveInCurrentSemester()) {
            // Группа не активна в текущем семестре - пропускаем
            qDebug() << "Group" << detail.number << "filtered: not active in current semester";
            return false;
        }
        
        if (!detail.hasActiveStudents()) {
            // В группе нет активных студентов - пропускаем
            qDebug() << "Group" << detail.number << "filtered: no active students";
            return false;
        }
    }

    const QString groupId = m_repo->upsertGroupFromPortal(
        detail.number, detail.admissionYear, detail.course, detail.specialty, detail.profile,
        detail.institute, detail.studyForm, detail.sourceUrl);
    if (groupId.isEmpty()) {
        qDebug() << "Failed to upsert group" << detail.number;
        return false;
    }

    qDebug() << "Group upserted with ID:" << groupId;
    ++m_groupsImported;
    const int studentsAdded = m_repo->mergePortalStudents(groupId, detail.students);
    m_studentsAdded += studentsAdded;
    qDebug() << "Merged" << studentsAdded << "students for group" << detail.number;
    return true;
}

void PortalImportService::processLocalHtmlFile(const QString& absolutePath, QSet<QString>& processed)
{
    const QString norm = QFileInfo(absolutePath).canonicalFilePath();
    if (norm.isEmpty() || processed.contains(norm))
        return;
    processed.insert(norm);

    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file:" << absolutePath;
        return;
    }

    const QString html = QString::fromUtf8(file.readAll());
    const QUrl baseUrl = QUrl::fromLocalFile(absolutePath);

    qDebug() << "Parsing HTML file, size:" << html.size() << "bytes";

    PortalGroupDetail detail;
    qDebug() << "Calling parseGroupPage...";
    const bool parseResult = NovsuHtmlParser::parseGroupPage(html, baseUrl, &detail);
    qDebug() << "parseGroupPage returned:" << parseResult;
    
    if (parseResult) {
        qDebug() << "Parsed group:" << detail.number << "Students:" << detail.students.size();
        detail.sourceUrl = baseUrl.toString();
        const bool applied = applyGroupDetail(detail);
        qDebug() << "Group applied:" << applied;
        return;
    }

    qDebug() << "Not a group page, looking for links...";
    const QList<QUrl> links = NovsuHtmlParser::extractGroupPageUrls(html, baseUrl);
    qDebug() << "Found links:" << links.size();
    for (const QUrl& link : links) {
        if (!link.isLocalFile())
            continue;
        const QString localPath = QFileInfo(link.toLocalFile()).canonicalFilePath();
        if (!localPath.isEmpty() && QFileInfo::exists(localPath))
            processLocalHtmlFile(localPath, processed);
    }
}

void PortalImportService::importFromHtmlDirectory(AcademicRepository* repo, const QString& directoryPath)
{
    qDebug() << "=== importFromHtmlDirectory CALLED ===";
    qDebug() << "Directory path:" << directoryPath;
    
    if (m_busy) {
        qDebug() << "Import already in progress, aborting";
        emit failed(tr("Импорт уже выполняется."));
        return;
    }
    if (!repo) {
        qDebug() << "Repository is null, aborting";
        emit failed(tr("Сессия не открыта."));
        return;
    }

    QDir dir(directoryPath);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    m_repo = repo;
    m_groupUrls.clear();
    m_groupIndex = 0;
    m_groupsImported = 0;
    m_studentsAdded = 0;
    m_step = Step::Idle;

    // ВРЕМЕННО отключаем фильтр для отладки
    const bool oldFilter = m_filterActiveOnly;
    m_filterActiveOnly = false;

    setBusy(true);
    setProgress(tr("Чтение HTML из папки…"), 5);

    const QStringList files =
        dir.entryList({QStringLiteral("*.html"), QStringLiteral("*.htm")}, QDir::Files, QDir::Name);
    
    qDebug() << "Found HTML files:" << files.size();
    
    QSet<QString> processed;
    for (int i = 0; i < files.size(); ++i) {
        const int pct = 5 + (files.isEmpty() ? 0 : (i * 90) / files.size());
        setProgress(tr("Файл %1 из %2…").arg(i + 1).arg(files.size()), pct);
        const QString filePath = dir.absoluteFilePath(files.at(i));
        qDebug() << "Processing file:" << filePath;
        processLocalHtmlFile(filePath, processed);
    }

    // Восстанавливаем фильтр
    m_filterActiveOnly = oldFilter;

    qDebug() << "Import finished. Groups:" << m_groupsImported << "Students:" << m_studentsAdded;

    if (m_groupsImported == 0) {
        m_step = Step::Idle;
        m_repo = nullptr;
        setBusy(false);
        const QString hint =
            tr("В папке пока нет HTML с группами. Сохраните страницы из браузера (Ctrl+S): %1")
                .arg(QDir::toNativeSeparators(dir.absolutePath()));
        qDebug() << "No groups imported. Hint:" << hint;
        emit progressChanged(hint, 0);
        emit finished(0, 0);
        return;
    }

    qDebug() << "Calling finishSuccess()";
    finishSuccess();
}

void PortalImportService::searchAndImportGroups(AcademicRepository* repo, const QString& sessionStorePath,
                                                const QStringList& groupPatterns)
{
    qDebug() << "=== searchAndImportGroups CALLED ===";
    qDebug() << "repo:" << (repo ? "valid" : "NULL");
    qDebug() << "sessionStorePath:" << sessionStorePath;
    qDebug() << "groupPatterns:" << groupPatterns;
    
    if (m_busy) {
        qDebug() << "ERROR: Already busy";
        emit failed(tr("Импорт уже выполняется."));
        return;
    }
    if (!repo) {
        qDebug() << "ERROR: No repo";
        emit failed(tr("Сессия не открыта."));
        return;
    }
    
    m_sessionStorePath = sessionStorePath;
    qDebug() << "Loading session cookies from:" << sessionStorePath;
    
    const bool loaded = loadSessionCookies();
    qDebug() << "Cookies loaded:" << loaded;
    
    if (!loaded) {
        qDebug() << "ERROR: Failed to load session cookies";
        emit failed(tr("Нет сохраненной сессии портала. Войдите сначала с логином и паролем, включив 'Запомнить сессию'."));
        return;
    }
    
    m_repo = repo;
    m_groupsImported = 0;
    m_studentsAdded = 0;
    m_searchIndex = 0;
    
    // Генерируем список номеров для поиска
    qDebug() << "Generating search numbers...";
    m_searchNumbers = generateGroupNumbers(groupPatterns);
    qDebug() << "Generated" << m_searchNumbers.size() << "numbers to search";
    
    if (m_searchNumbers.size() <= 10) {
        qDebug() << "Search list:" << m_searchNumbers;
    } else {
        qDebug() << "First 10:" << m_searchNumbers.mid(0, 10);
    }
    
    if (m_searchNumbers.isEmpty()) {
        qDebug() << "ERROR: No search numbers";
        emit failed(tr("Не указаны паттерны для поиска групп."));
        return;
    }
    
    setBusy(true);
    setProgress(tr("Автопоиск групп: будет проверено %1 номеров...").arg(m_searchNumbers.size()), 0);
    
    // Начинаем поиск
    m_step = Step::LoadingGroup;
    qDebug() << "Starting search...";
    searchNextGroupNumber();
}

QStringList PortalImportService::generateGroupNumbers(const QStringList& patterns) const
{
    QStringList numbers;
    
    if (patterns.isEmpty()) {
        // Паттерны по умолчанию: все возможные номера групп
        // Группы: 4-значные номера от 0100 до 9999
        // Первая цифра: 0-9 (год поступления или специальность)
        // Остальные: 100-999 (номер группы/специальности)
        
        // Генерируем только активные диапазоны для уменьшения количества запросов
        // Обычно группы с номерами X900-X999 и X000-X099 редки
        for (int first = 0; first <= 9; ++first) {
            for (int rest = 100; rest <= 899; ++rest) {
                numbers << QString("%1%2").arg(first).arg(rest, 3, 10, QChar('0'));
            }
        }
    } else {
        // Используем заданные паттерны
        for (const QString& pattern : patterns) {
            if (pattern.contains(QStringLiteral("*"))) {
                // Паттерн с маской
                QString prefix = pattern;
                prefix.remove(QStringLiteral("*"));
                
                if (prefix.isEmpty()) {
                    // "*" = все группы от 0100 до 9999
                    for (int i = 100; i <= 9999; ++i) {
                        numbers << QString("%1").arg(i, 4, 10, QChar('0'));
                    }
                } else if (prefix.length() == 1) {
                    // "2*" = 2100-2999
                    const int first = prefix.toInt();
                    for (int rest = 100; rest <= 999; ++rest) {
                        numbers << QString("%1%2").arg(first).arg(rest, 3, 10, QChar('0'));
                    }
                } else if (prefix.length() == 2) {
                    // "29*" = 2900-2999
                    const int base = prefix.toInt() * 100;
                    for (int i = 0; i < 100; ++i) {
                        numbers << QString("%1").arg(base + i, 4, 10, QChar('0'));
                    }
                } else if (prefix.length() == 3) {
                    // "299*" = 2990-2999
                    const int base = prefix.toInt() * 10;
                    for (int i = 0; i < 10; ++i) {
                        numbers << QString("%1").arg(base + i, 4, 10, QChar('0'));
                    }
                }
            } else if (pattern.contains(QStringLiteral("-"))) {
                // Диапазон, например "2900-2950"
                const QStringList parts = pattern.split(QStringLiteral("-"));
                if (parts.size() == 2) {
                    const int start = parts[0].toInt();
                    const int end = parts[1].toInt();
                    for (int i = start; i <= end; ++i) {
                        numbers << QString("%1").arg(i, 4, 10, QChar('0'));
                    }
                }
            } else {
                // Конкретный номер - дополняем до 4 цифр если нужно
                const QString num = pattern.trimmed();
                if (num.length() < 4) {
                    numbers << QString("%1").arg(num.toInt(), 4, 10, QChar('0'));
                } else {
                    numbers << num;
                }
            }
        }
    }
    
    return numbers;
}

void PortalImportService::searchNextGroupNumber()
{
    qDebug() << "=== searchNextGroupNumber CALLED ===";
    qDebug() << "m_searchIndex:" << m_searchIndex << "/ total:" << m_searchNumbers.size();
    
    if (!m_repo || m_searchIndex >= m_searchNumbers.size()) {
        qDebug() << "Search finished. Calling finishSuccess()";
        finishSuccess();
        return;
    }
    
    const QString groupNumber = m_searchNumbers.at(m_searchIndex);
    const int pct = 5 + (m_searchIndex * 90) / qMax(1, m_searchNumbers.size());
    setProgress(tr("Проверка группы %1 (%2/%3)...").arg(groupNumber).arg(m_searchIndex + 1).arg(m_searchNumbers.size()), pct);
    
    qDebug() << "Searching for group:" << groupNumber;
    
    // Используем формат поиска портала НовГУ
    // Формат: https://portal.novsu.ru/search/groups/i.2500/?page=search&grpname=НОМЕР
    const QString searchUrl = QStringLiteral("https://portal.novsu.ru/search/groups/i.2500/?page=search&grpname=") + groupNumber;
    
    qDebug() << "Search URL:" << searchUrl;
    
    beginRequest(QUrl(searchUrl));
    qDebug() << "Request sent";
}

void PortalImportService::handleGroupPage(const QByteArray& body, const QUrl& url)
{
    qDebug() << "=== handleGroupPage CALLED ===";
    qDebug() << "URL:" << url.toString();
    qDebug() << "Body size:" << body.size();
    qDebug() << "Is auto search:" << !m_searchNumbers.isEmpty();
    
    PortalGroupDetail detail;
    const QString html = QString::fromUtf8(body);
    
    qDebug() << "Parsing HTML...";
    if (!NovsuHtmlParser::parseGroupPage(html, url, &detail)) {
        qDebug() << "Group NOT found or parse failed";
        // Группа не найдена или ошибка парсинга
        if (m_step == Step::LoadingGroup && !m_searchNumbers.isEmpty()) {
            qDebug() << "Auto search - continuing to next number";
            // Это автопоиск - продолжаем со следующим номером
            ++m_searchIndex;
            QTimer::singleShot(100, this, &PortalImportService::searchNextGroupNumber);
        } else {
            qDebug() << "Regular sync - continuing to next group";
            // Обычная синхронизация
            ++m_groupIndex;
            QTimer::singleShot(200, this, &PortalImportService::scheduleNextGroup);
        }
        return;
    }

    qDebug() << "Group FOUND!";
    qDebug() << "Group number:" << detail.number;
    qDebug() << "Students count:" << detail.students.size();
    
    detail.sourceUrl = url.toString(QUrl::RemoveFragment);
    qDebug() << "Applying group detail...";
    applyGroupDetail(detail);

    if (!m_searchNumbers.isEmpty()) {
        qDebug() << "Auto search - next number in 100ms";
        // Автопоиск - продолжаем
        ++m_searchIndex;
        QTimer::singleShot(100, this, &PortalImportService::searchNextGroupNumber);
    } else {
        qDebug() << "Regular sync - next group in 250ms";
        // Обычная синхронизация
        ++m_groupIndex;
        QTimer::singleShot(250, this, &PortalImportService::scheduleNextGroup);
    }
    
    qDebug() << "=== handleGroupPage FINISHED ===";
}

void PortalImportService::onReplyFinished(QNetworkReply* reply)
{
    qDebug() << "=== onReplyFinished CALLED ===";
    qDebug() << "reply:" << reply;
    qDebug() << "m_busy:" << m_busy;
    
    reply->deleteLater();
    if (!m_busy) {
        qDebug() << "Not busy, ignoring reply";
        return;
    }

    qDebug() << "Reply URL:" << reply->url().toString();
    qDebug() << "Reply error:" << reply->error() << reply->errorString();
    qDebug() << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Network error, calling fail()";
        fail(tr("Ошибка сети: %1").arg(reply->errorString()));
        return;
    }

    const QByteArray body = reply->readAll();
    const QUrl url = reply->url();
    
    qDebug() << "Body size:" << body.size() << "bytes";
    qDebug() << "Current step:" << static_cast<int>(m_step);

    switch (m_step) {
    case Step::LoadingHome:
        qDebug() << "Routing to handleHomePage()";
        handleHomePage(body, url);
        break;
    case Step::PostingLogin:
        qDebug() << "Routing to handleLoginResult()";
        handleLoginResult();
        break;
    case Step::CheckingSession:
    case Step::LoadingIndex:
        qDebug() << "Routing to handleIndexPage()";
        handleIndexPage(body, url);
        break;
    case Step::LoadingGroup:
        qDebug() << "Routing to handleGroupPage()";
        handleGroupPage(body, url);
        break;
    default:
        qDebug() << "Unknown step, ignoring";
        break;
    }
    
    qDebug() << "=== onReplyFinished FINISHED ===";
}

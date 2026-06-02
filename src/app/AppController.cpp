#include "AppController.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QNetworkInformation>
#include <QStandardPaths>
#include <QTextStream>
#include <QDateTime>
#include <QTimer>
#include <QUrl>

#include "../portal/PortalImportService.h"
#include "../portal/PortalSessionStore.h"
#include "../storage/AcademicRepository.h"

#include <QSettings>

AppController::AppController(QObject* parent)
    : QObject(parent)
{
    m_groupsModel = new GroupListModel(this);
    m_studentsModel = new StudentListModel(this);
    m_disciplinesModel = new DisciplineListModel(this);
    m_gradeSheetsModel = new GradeSheetListModel(this);
    m_portalImport = new PortalImportService(this);

    QSettings settings;
    m_portalGroupsIndexUrl = settings.value(QStringLiteral("portal/groupsIndexUrl")).toString();
    m_portalAutoImport = settings.value(QStringLiteral("portal/autoImportLocal"), true).toBool();
    m_portalOnlineAutoSync = settings.value(QStringLiteral("portal/onlineAutoSync"), false).toBool();
    m_portalSavedLogin = settings.value(QStringLiteral("portal/savedLogin")).toString();

    m_portalImportDebounce = new QTimer(this);
    m_portalImportDebounce->setSingleShot(true);
    m_portalImportDebounce->setInterval(1500);
    connect(m_portalImportDebounce, &QTimer::timeout, this, &AppController::importGroupsFromLocalHtml);

    // Таймер для периодической синхронизации (раз в день)
    m_periodicSyncTimer = new QTimer(this);
    m_periodicSyncTimer->setInterval(24 * 60 * 60 * 1000); // 24 часа
    connect(m_periodicSyncTimer, &QTimer::timeout, this, &AppController::onPeriodicSyncTimer);

    connect(m_portalImport, &PortalImportService::busyChanged, this, [this](bool busy) {
        m_portalSyncBusy = busy;
        emit portalSyncChanged();
    });
    connect(m_portalImport, &PortalImportService::progressChanged, this, [this](const QString& msg, int pct) {
        m_portalSyncMessage = msg;
        m_portalSyncPercent = pct;
        emit portalSyncChanged();
    });
    connect(m_portalImport, &PortalImportService::finished, this, [this](int groups, int students) {
        m_portalSyncMessage = tr("Импортировано групп: %1, добавлено студентов: %2").arg(groups).arg(students);
        m_portalSyncPercent = 100;
        qDebug() << "Portal import finished. Calling reloadGroups()";
        reloadGroups();
        qDebug() << "Groups reloaded. Emitting signals.";
        emit portalSyncChanged();
        emit portalSyncFinished(groups, students);
        emit sessionOpenChanged();
    });
    connect(m_portalImport, &PortalImportService::failed, this, [this](const QString& msg) {
        setError(msg);
        m_portalSyncMessage = msg;
        m_portalSyncPercent = 0;
        emit portalSyncChanged();
    });

    initNetworkMonitoring();
}

AppController::~AppController()
{
    tearDownSession();
}

void AppController::initNetworkMonitoring()
{
    if (QNetworkInformation::loadDefaultBackend()) {
        if (QNetworkInformation* ni = QNetworkInformation::instance()) {
            connect(ni, &QNetworkInformation::reachabilityChanged, this,
                    [this](QNetworkInformation::Reachability r) {
                        setOnline(r == QNetworkInformation::Reachability::Online);
                    });
            setOnline(ni->reachability() == QNetworkInformation::Reachability::Online);
            return;
        }
    }
    setOnline(true);
}

void AppController::setOnline(bool online)
{
    if (m_isOnline == online)
        return;
    m_isOnline = online;
    emit isOnlineChanged();
}

void AppController::tearDownSession()
{
    if (m_portalWatcher) {
        m_portalWatcher->removePaths(m_portalWatcher->directories());
        m_portalWatcher->removePaths(m_portalWatcher->files());
    }

    delete m_repo;
    m_repo = nullptr;
    m_userDataPath.clear();

    m_actorLogin.clear();
    m_actorDisplayName.clear();
    m_actorRole.clear();
    m_activeGroupId.clear();

    m_groupsModel->setGroups({});
    m_studentsModel->setStudents({});
    m_gradeSheetsModel->setSheets({});

    emit groupsChanged();
    emit studentsChanged();
    emit gradeSheetsChanged();
    emit sessionOpenChanged();
}

void AppController::openSession(const QString& userDataAbsolutePath, const QString& actorLogin,
                                const QString& actorDisplayName, const QString& actorRole)
{
    tearDownSession();

    if (userDataAbsolutePath.isEmpty() || actorLogin.isEmpty())
        return;

    QDir().mkpath(userDataAbsolutePath);
    m_actorLogin = actorLogin;
    m_actorDisplayName = actorDisplayName;
    m_actorRole = actorRole.isEmpty() ? QStringLiteral("teacher") : actorRole;

    m_userDataPath = userDataAbsolutePath;
    m_repo = new AcademicRepository(userDataAbsolutePath + QStringLiteral("/academic.sqlite"), this);
    reloadGroups();
    reloadDisciplines();
    setupPortalImportWatch();
    emit sessionOpenChanged();

    if (m_portalAutoImport)
        scheduleLocalPortalImport();
    if (m_portalOnlineAutoSync) {
        scheduleOnlinePortalSync();
        schedulePeriodicSync(); // Запускаем периодическую синхронизацию
    }
}

QString AppController::portalSessionFilePath() const
{
    if (m_userDataPath.isEmpty())
        return {};
    return m_userDataPath + QStringLiteral("/portal_cookies.json");
}

bool AppController::portalHasSavedSession() const
{
    return PortalSessionStore::exists(portalSessionFilePath());
}

void AppController::closeSession()
{
    tearDownSession();
}

void AppController::setError(const QString& msg)
{
    m_lastError = msg;
    emit lastErrorChanged();
}

void AppController::clearError()
{
    if (m_lastError.isEmpty())
        return;
    m_lastError.clear();
    emit lastErrorChanged();
}

void AppController::reloadGroups()
{
    if (!m_repo) {
        qDebug() << "reloadGroups: no repository";
        return;
    }
    qDebug() << "reloadGroups: loading groups from database";
    const auto groups = m_repo->listGroups();
    qDebug() << "reloadGroups: found" << groups.size() << "groups";
    m_groupsModel->setGroups(groups);
    emit groupsChanged();
    emit pendingSyncTasksChanged();
}

bool AppController::createGroup(const QString& number)
{
    if (!m_repo)
        return false;
    clearError();
    if (!m_repo->createGroup(number)) {
        setError(tr("Не удалось создать группу. Проверьте номер."));
        return false;
    }
    reloadGroups();
    return true;
}

bool AppController::deleteGroup(const QString& groupId)
{
    if (!m_repo)
        return false;
    clearError();
    if (!m_repo->deleteGroup(groupId)) {
        setError(tr("Не удалось удалить группу."));
        return false;
    }
    if (m_activeGroupId == groupId)
        m_activeGroupId.clear();
    reloadGroups();
    return true;
}

void AppController::reloadStudents(const QString& groupId)
{
    if (!m_repo)
        return;
    m_activeGroupId = groupId;
    m_studentsModel->setStudents(m_repo->listStudents(groupId));
    emit studentsChanged();
}

bool AppController::addStudent(const QString& groupId, const QString& studentNumber, const QString& fio, const QString& status)
{
    if (!m_repo)
        return false;
    clearError();
    if (!m_repo->addStudent(groupId, studentNumber, fio, status)) {
        setError(tr("Не удалось добавить студента."));
        return false;
    }
    reloadStudents(groupId);
    reloadGroups();
    return true;
}

bool AppController::removeStudent(const QString& studentId)
{
    if (!m_repo)
        return false;
    clearError();
    const QString gid = m_activeGroupId;
    if (!m_repo->removeStudent(studentId)) {
        setError(tr("Не удалось удалить студента."));
        return false;
    }
    if (!gid.isEmpty()) {
        reloadStudents(gid);
        reloadGroups();
    }
    return true;
}

bool AppController::updateStudent(const QString& studentId, const QString& fio,
                                   const QString& status, const QString& grade)
{
    if (!m_repo)
        return false;
    clearError();
    if (!m_repo->updateStudent(studentId, fio, status, grade)) {
        setError(tr("Не удалось обновить данные студента."));
        return false;
    }
    if (!m_activeGroupId.isEmpty())
        reloadStudents(m_activeGroupId);
    return true;
}

int AppController::importStudents(const QString& groupId, const QString& linesText)
{
    if (!m_repo)
        return 0;
    clearError();
    const int n = m_repo->importStudentsFromLines(groupId, linesText);
    if (n <= 0)
        setError(tr("Формат: номер/ФИО/статус — по одной строке."));
    reloadStudents(groupId);
    reloadGroups();
    return n;
}

void AppController::reloadDisciplines()
{
    if (!m_repo)
        return;
    m_disciplinesModel->setDisciplines(m_repo->listDisciplines());
    emit disciplinesChanged();
}

bool AppController::addDiscipline(const QString& name)
{
    if (!m_repo)
        return false;
    clearError();
    if (!m_repo->addDiscipline(name)) {
        setError(tr("Не удалось добавить дисциплину."));
        return false;
    }
    reloadDisciplines();
    return true;
}

void AppController::reloadGradeSheets(const QString& groupId)
{
    if (!m_repo)
        return;
    m_gradeSheetsModel->setSheets(m_repo->listGradeSheets(groupId));
    emit gradeSheetsChanged();
}

QString AppController::createGradeSheet(const QString& groupId, const QString& disciplineId, 
                                        const QString& examType)
{
    if (!m_repo)
        return {};
    clearError();
    
    // Если тип не указан, используем "exam" по умолчанию
    QString type = examType.trimmed();
    if (type.isEmpty())
        type = QStringLiteral("exam");
    
    QString sheetId;
    if (!m_repo->createGradeSheet(groupId, disciplineId, type, &sheetId)) {
        setError(tr("Не удалось создать ведомость."));
        return {};
    }
    reloadGradeSheets(groupId);
    reloadGroups();
    return sheetId;
}

QVariantMap AppController::gradeSheetInfo(const QString& sheetId) const
{
    QVariantMap out;
    if (!m_repo)
        return out;
    const GradeSheet s = m_repo->findGradeSheet(sheetId);
    if (s.id.isEmpty())
        return out;
    out.insert(QStringLiteral("sheetId"), s.id);
    out.insert(QStringLiteral("groupId"), s.groupId);
    out.insert(QStringLiteral("disciplineId"), s.disciplineId);
    out.insert(QStringLiteral("title"), s.title);
    out.insert(QStringLiteral("disciplineName"), s.disciplineName);
    out.insert(QStringLiteral("examType"), s.examType);
    out.insert(QStringLiteral("createdAt"), s.createdAt);
    return out;
}

void AppController::flushSync()
{
    reloadGroups();
}

void AppController::setPortalGroupsIndexUrl(const QString& url)
{
    const QString trimmed = url.trimmed();
    if (m_portalGroupsIndexUrl == trimmed)
        return;
    m_portalGroupsIndexUrl = trimmed;
    QSettings settings;
    settings.setValue(QStringLiteral("portal/groupsIndexUrl"), trimmed);
    emit portalSettingsChanged();
}

void AppController::setPortalOnlineAutoSync(bool enabled)
{
    if (m_portalOnlineAutoSync == enabled)
        return;
    m_portalOnlineAutoSync = enabled;
    QSettings settings;
    settings.setValue(QStringLiteral("portal/onlineAutoSync"), enabled);
    emit portalSettingsChanged();
    
    // Управление периодическим таймером
    if (enabled && m_repo) {
        schedulePeriodicSync();
    } else if (m_periodicSyncTimer) {
        m_periodicSyncTimer->stop();
    }
}

void AppController::setPortalSavedLogin(const QString& login)
{
    const QString trimmed = login.trimmed();
    if (m_portalSavedLogin == trimmed)
        return;
    m_portalSavedLogin = trimmed;
    QSettings settings;
    settings.setValue(QStringLiteral("portal/savedLogin"), trimmed);
    emit portalSettingsChanged();
}

void AppController::setPortalAutoImport(bool enabled)
{
    if (m_portalAutoImport == enabled)
        return;
    m_portalAutoImport = enabled;
    QSettings settings;
    settings.setValue(QStringLiteral("portal/autoImportLocal"), enabled);
    emit portalSettingsChanged();
    if (enabled && m_repo)
        setupPortalImportWatch();
}

QString AppController::portalLocalImportDir() const
{
    if (m_userDataPath.isEmpty())
        return {};
    return m_userDataPath + QStringLiteral("/portal-html");
}

void AppController::setupPortalImportWatch()
{
    const QString dir = portalLocalImportDir();
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);

    if (!m_portalWatcher) {
        m_portalWatcher = new QFileSystemWatcher(this);
        connect(m_portalWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
            if (m_portalAutoImport && m_repo)
                m_portalImportDebounce->start();
        });
    }

    const QStringList watched = m_portalWatcher->directories();
    if (!watched.contains(dir))
        m_portalWatcher->addPath(dir);
}

void AppController::scheduleLocalPortalImport()
{
    QTimer::singleShot(400, this, &AppController::importGroupsFromLocalHtml);
}

void AppController::scheduleOnlinePortalSync()
{
    if (!m_isOnline || m_portalGroupsIndexUrl.isEmpty() || !portalHasSavedSession())
        return;
    QTimer::singleShot(1200, this, &AppController::syncPortalOnlineSavedSession);
}

void AppController::syncPortalOnlineSavedSession()
{
    syncGroupsFromPortal(QString(), QString());
}

void AppController::clearPortalSession()
{
    PortalSessionStore::remove(portalSessionFilePath());
    emit sessionOpenChanged();
}

void AppController::importGroupsFromLocalHtml()
{
    qDebug() << "=== AppController::importGroupsFromLocalHtml CALLED ===";
    qDebug() << "m_repo:" << (m_repo ? "valid" : "NULL");
    qDebug() << "portalImport busy:" << (m_portalImport ? m_portalImport->busy() : false);
    
    if (!m_repo || m_portalImport->busy()) {
        qDebug() << "Aborting import: repo or busy check failed";
        return;
    }
    clearError();
    
    const QString importDir = portalLocalImportDir();
    qDebug() << "Import directory:" << importDir;
    
    m_portalSyncMessage = tr("Импорт из папки…");
    m_portalSyncPercent = 0;
    emit portalSyncChanged();
    m_portalImport->importFromHtmlDirectory(m_repo, importDir);
}

void AppController::openPortalImportFolder()
{
    const QString dir = portalLocalImportDir();
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void AppController::syncGroupsFromPortal(const QString& portalLogin, const QString& portalPassword)
{
    if (!m_repo) {
        setError(tr("Сначала войдите в приложение."));
        return;
    }

    const bool useSavedSession = portalLogin.trimmed().isEmpty() && portalPassword.isEmpty();
    if (useSavedSession) {
        if (portalHasSavedSession() && !m_portalGroupsIndexUrl.isEmpty()) {
            if (!m_isOnline) {
                setError(tr("Нет подключения к интернету."));
                return;
            }
        } else {
            importGroupsFromLocalHtml();
            return;
        }
    }

    if (!m_isOnline) {
        setError(tr("Нет подключения к интернету."));
        return;
    }
    if (m_portalGroupsIndexUrl.isEmpty()) {
        setError(tr("Укажите URL страницы со списком групп на портале."));
        return;
    }

    clearError();
    m_portalSyncMessage = useSavedSession ? tr("Синхронизация по сохранённой сессии…") : tr("Запуск…");
    m_portalSyncPercent = 0;
    emit portalSyncChanged();
    m_portalImport->startSync(m_repo, portalLogin, portalPassword, m_portalGroupsIndexUrl,
                              portalSessionFilePath());
}

void AppController::autoSearchGroups(const QStringList& patterns)
{
    qDebug() << "=== autoSearchGroups CALLED ===";
    qDebug() << "Patterns:" << patterns;
    
    if (!m_repo) {
        qDebug() << "ERROR: No repository";
        setError(tr("Сначала войдите в приложение."));
        return;
    }
    
    qDebug() << "Checking saved session...";
    qDebug() << "portalSessionFilePath:" << portalSessionFilePath();
    qDebug() << "portalHasSavedSession:" << portalHasSavedSession();
    
    // Если нет сохранённой сессии - пробуем войти с логином/паролем
    if (!portalHasSavedSession()) {
        qDebug() << "No saved session";
        qDebug() << "portalSavedLogin:" << m_portalSavedLogin;
        
        // Пытаемся получить логин из настроек
        if (m_portalSavedLogin.isEmpty()) {
            qDebug() << "ERROR: No saved login";
            setError(tr("Нет сохраненной сессии портала и логина. Войдите один раз через онлайн синхронизацию."));
            return;
        }
        
        qDebug() << "ERROR: Need password";
        // Спрашиваем пароль у пользователя через ошибку
        setError(tr("Нет сохраненной сессии. Введите логин и пароль в разделе 'Онлайн синхронизация' и нажмите 'Синхронизировать'."));
        return;
    }
    
    clearError();
    qDebug() << "Starting search with saved session...";
    m_portalSyncMessage = tr("Автопоиск групп...");
    m_portalSyncPercent = 0;
    emit portalSyncChanged();
    
    qDebug() << "Calling searchAndImportGroups()...";
    m_portalImport->searchAndImportGroups(m_repo, portalSessionFilePath(), patterns);
    qDebug() << "searchAndImportGroups() returned";
}

QString AppController::exportGradeSheetHtml(const QString& sheetId, const QString& baseName) const
{
    if (!m_repo)
        return {};

    const GradeSheet sheet = m_repo->findGradeSheet(sheetId);
    if (sheet.id.isEmpty())
        return {};

    // Получаем студентов группы
    const QVector<Student> students = m_repo->listStudents(sheet.groupId);

    // Формируем HTML
    QString html;
    QTextStream out(&html);
    out << "<!DOCTYPE html>\n<html lang=\"ru\">\n<head>\n"
        << "<meta charset=\"UTF-8\">\n"
        << "<title>Аттестационная ведомость</title>\n"
        << "<style>\n"
        << "body { font-family: Arial, sans-serif; font-size: 12px; margin: 20px; }\n"
        << ".header { text-align: center; margin-bottom: 20px; }\n"
        << ".header h3 { margin: 4px 0; }\n"
        << "table { width: 100%; border-collapse: collapse; }\n"
        << "th, td { border: 1px solid #000; padding: 4px 8px; }\n"
        << "th { background: #f0f0f0; font-weight: bold; text-align: center; }\n"
        << ".footer { margin-top: 20px; }\n"
        << "@media print { button { display: none; } }\n"
        << "</style>\n</head>\n<body>\n";

    out << "<div class=\"header\">\n"
        << "<p>Министерство науки и высшего образования Российской Федерации</p>\n"
        << "<p>Федеральное государственное бюджетное образовательное учреждение высшего образования</p>\n"
        << "<h3>ПОЛИТЕХНИЧЕСКИЙ ИНСТИТУТ<br>ПОЛИТЕХНИЧЕСКИЙ КОЛЛЕДЖ</h3>\n"
        << "<h2>Аттестационная ведомость</h2>\n"
        << "<p><b>Дисциплина:</b> " << sheet.disciplineName.toHtmlEscaped() << "</p>\n"
        << "<p><b>Группа:</b> " << sheet.groupId.toHtmlEscaped()
        << " &nbsp;&nbsp; <b>Дата:</b> "
        << QDateTime::fromMSecsSinceEpoch(sheet.createdAt).toString("dd.MM.yyyy")
        << "</p>\n"
        << "</div>\n";

    out << "<table>\n<thead>\n<tr>"
        << "<th>№ п/п</th>"
        << "<th>Фамилия, И.О. студента</th>"
        << "<th>Отметка о допуске</th>"
        << "<th>Оценка</th>"
        << "<th>Подпись преподавателя</th>"
        << "</tr>\n</thead>\n<tbody>\n";

    int idx = 1;
    for (const auto& s : students) {
        out << "<tr>"
            << "<td style=\"text-align:center\">" << idx++ << "</td>"
            << "<td>" << s.fio.toHtmlEscaped() << "</td>"
            << "<td style=\"text-align:center\">"
            << (s.status == QStringLiteral("обучается") ? "доп." : s.status.toHtmlEscaped())
            << "</td>"
            << "<td style=\"text-align:center\">&nbsp;</td>"
            << "<td>&nbsp;</td>"
            << "</tr>\n";
    }

    out << "</tbody>\n</table>\n"
        << "<div class=\"footer\">\n"
        << "<p>* Оценки проставляются цифрами и в скобках прописью</p>\n"
        << "<br><p>Подпись преподавателя ___________________</p>\n"
        << "<p>Зам. директора по УМ и ВР ___________________</p>\n"
        << "</div>\n"
        << "<button onclick=\"window.print()\">Печать</button>\n"
        << "</body>\n</html>\n";

    // Сохраняем файл
    const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString fileName = desktopPath + QStringLiteral("/") + baseName + QStringLiteral(".html");

    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return {};

    QTextStream fileOut(&f);
    fileOut << html;
    f.close();

    return fileName;
}

bool AppController::submitGradeSheetForApproval(const QString& sheetId)
{
    if (!m_repo)
        return false;
    const bool ok = m_repo->setGradeSheetStatus(sheetId, QStringLiteral("pending_approval"));
    if (ok) emit pendingSheetsChanged();
    return ok;
}

bool AppController::approveGradeSheet(const QString& sheetId)
{
    if (!m_repo)
        return false;
    const bool ok = m_repo->approveGradeSheet(sheetId, m_actorLogin);
    if (ok) emit pendingSheetsChanged();
    return ok;
}

bool AppController::rejectGradeSheet(const QString& sheetId)
{
    if (!m_repo)
        return false;
    const bool ok = m_repo->setGradeSheetStatus(sheetId, QStringLiteral("draft"));
    if (ok) emit pendingSheetsChanged();
    return ok;
}

QVariantList AppController::pendingSheetsList() const
{
    QVariantList out;
    if (!m_repo)
        return out;
    const auto sheets = m_repo->listPendingSheets();
    for (const auto& s : sheets) {
        QVariantMap m;
        m.insert(QStringLiteral("sheetId"), s.id);
        m.insert(QStringLiteral("groupId"), s.groupId);
        m.insert(QStringLiteral("groupNumber"), s.groupNumber);
        m.insert(QStringLiteral("disciplineName"), s.disciplineName);
        m.insert(QStringLiteral("title"), s.title);
        m.insert(QStringLiteral("createdAt"), s.createdAt);
        m.insert(QStringLiteral("status"), s.status);
        out.append(m);
    }
    return out;
}

void AppController::reloadPendingSheets()
{
    emit pendingSheetsChanged();
}

void AppController::schedulePeriodicSync()
{
    if (!m_periodicSyncTimer || !m_portalOnlineAutoSync || !m_repo)
        return;
    
    // Запускаем таймер для синхронизации раз в день
    m_periodicSyncTimer->start();
}

void AppController::onPeriodicSyncTimer()
{
    // Автоматическая синхронизация раз в день
    if (!m_isOnline || !m_repo || !m_portalOnlineAutoSync)
        return;
    
    if (portalHasSavedSession() && !m_portalGroupsIndexUrl.isEmpty()) {
        syncPortalOnlineSavedSession();
    }
}

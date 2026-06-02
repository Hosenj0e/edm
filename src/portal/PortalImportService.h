#pragma once

#include <QObject>
#include <QSet>
#include <QUrl>

#include "PortalTypes.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkCookieJar;
class AcademicRepository;

class PortalImportService : public QObject
{
    Q_OBJECT

public:
    explicit PortalImportService(QObject* parent = nullptr);
    ~PortalImportService() override;

    void startSync(AcademicRepository* repo, const QString& login, const QString& password,
                   const QString& groupsIndexUrl, const QString& sessionStorePath = {});

    /// Импорт без сети и пароля: HTML-файлы групп в папке (сохранённые из браузера).
    void importFromHtmlDirectory(AcademicRepository* repo, const QString& directoryPath);
    
    /// Автоматический поиск групп по паттернам номеров (перебор)
    void searchAndImportGroups(AcademicRepository* repo, const QString& sessionStorePath,
                              const QStringList& groupPatterns = {});

    bool busy() const { return m_busy; }
    
    /// Включить/выключить фильтрацию только активных групп
    void setFilterActiveOnly(bool enabled) { m_filterActiveOnly = enabled; }
    bool filterActiveOnly() const { return m_filterActiveOnly; }

signals:
    void busyChanged(bool busy);
    void progressChanged(const QString& message, int percent);
    void finished(int groupsImported, int studentsAdded);
    void failed(const QString& message);

private:
    enum class Step {
        Idle,
        LoadingHome,
        PostingLogin,
        LoadingIndex,
        CheckingSession,
        LoadingGroup,
        Done
    };

    void setBusy(bool busy);
    void setProgress(const QString& message, int percent);
    void fail(const QString& message);
    void finishSuccess();

    void beginRequest(const QUrl& url);
    void postLogin(const PortalLoginForm& form);
    void handleHomePage(const QByteArray& body, const QUrl& url);
    void handleLoginResult();
    void handleIndexPage(const QByteArray& body, const QUrl& url);
    void saveSessionCookies();
    bool loadSessionCookies();
    bool pageLooksLikeLogin(const QString& html) const;
    void handleGroupPage(const QByteArray& body, const QUrl& url);
    void scheduleNextGroup();
    bool applyGroupDetail(const PortalGroupDetail& detail);
    void processLocalHtmlFile(const QString& absolutePath, QSet<QString>& processed);

    void onReplyFinished(QNetworkReply* reply);
    
    // Для автопоиска групп
    void searchNextGroupNumber();
    QStringList generateGroupNumbers(const QStringList& patterns) const;

    QNetworkAccessManager* m_nam = nullptr;
    QNetworkCookieJar* m_jar = nullptr;
    AcademicRepository* m_repo = nullptr;

    QString m_login;
    QString m_password;
    QString m_indexUrl;
    QString m_sessionStorePath;
    QUrl m_baseUrl = QUrl(QStringLiteral("https://portal.novsu.ru/"));

    Step m_step = Step::Idle;
    bool m_busy = false;
    QList<QUrl> m_groupUrls;
    int m_groupIndex = 0;
    int m_groupsImported = 0;
    int m_studentsAdded = 0;
    bool m_filterActiveOnly = true; // По умолчанию импортируем только активные группы
    
    // Для автопоиска
    QStringList m_searchNumbers;
    int m_searchIndex = 0;
};

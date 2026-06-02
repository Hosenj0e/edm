#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class QFileSystemWatcher;
class QTimer;

#include "../ui/model/GroupListModel.h"
#include "../ui/model/StudentListModel.h"
#include "../ui/model/DisciplineListModel.h"
#include "../ui/model/GradeSheetListModel.h"

class AcademicRepository;
class PortalImportService;

class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool sessionOpen READ sessionOpen NOTIFY sessionOpenChanged)
    Q_PROPERTY(QString currentUserLogin READ currentUserLogin NOTIFY sessionOpenChanged)
    Q_PROPERTY(QString currentUserDisplayName READ currentUserDisplayName NOTIFY sessionOpenChanged)
    Q_PROPERTY(QString currentUserRole READ currentUserRole NOTIFY sessionOpenChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY isOnlineChanged)
    Q_PROPERTY(int pendingSyncTasks READ pendingSyncTasks NOTIFY pendingSyncTasksChanged)

    Q_PROPERTY(GroupListModel* groupsModel READ groupsModel NOTIFY groupsChanged)
    Q_PROPERTY(StudentListModel* studentsModel READ studentsModel NOTIFY studentsChanged)
    Q_PROPERTY(DisciplineListModel* disciplinesModel READ disciplinesModel NOTIFY disciplinesChanged)
    Q_PROPERTY(GradeSheetListModel* gradeSheetsModel READ gradeSheetsModel NOTIFY gradeSheetsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool portalSyncBusy READ portalSyncBusy NOTIFY portalSyncChanged)
    Q_PROPERTY(QString portalSyncMessage READ portalSyncMessage NOTIFY portalSyncChanged)
    Q_PROPERTY(int portalSyncPercent READ portalSyncPercent NOTIFY portalSyncChanged)
    Q_PROPERTY(QString portalGroupsIndexUrl READ portalGroupsIndexUrl WRITE setPortalGroupsIndexUrl NOTIFY portalSettingsChanged)
    Q_PROPERTY(QString portalLocalImportDir READ portalLocalImportDir NOTIFY sessionOpenChanged)
    Q_PROPERTY(bool portalAutoImport READ portalAutoImport WRITE setPortalAutoImport NOTIFY portalSettingsChanged)
    Q_PROPERTY(bool portalHasSavedSession READ portalHasSavedSession NOTIFY sessionOpenChanged)
    Q_PROPERTY(bool portalOnlineAutoSync READ portalOnlineAutoSync WRITE setPortalOnlineAutoSync NOTIFY portalSettingsChanged)
    Q_PROPERTY(QString portalSavedLogin READ portalSavedLogin WRITE setPortalSavedLogin NOTIFY portalSettingsChanged)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    bool sessionOpen() const { return m_repo != nullptr; }
    QString currentUserLogin() const { return m_actorLogin; }
    QString currentUserDisplayName() const { return m_actorDisplayName; }
    QString currentUserRole() const { return m_actorRole; }
    bool isOnline() const { return m_isOnline; }
    int pendingSyncTasks() const { return 0; }

    GroupListModel* groupsModel() const { return m_groupsModel; }
    StudentListModel* studentsModel() const { return m_studentsModel; }
    DisciplineListModel* disciplinesModel() const { return m_disciplinesModel; }
    GradeSheetListModel* gradeSheetsModel() const { return m_gradeSheetsModel; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void openSession(const QString& userDataAbsolutePath, const QString& actorLogin,
                                 const QString& actorDisplayName, const QString& actorRole = {});
    Q_INVOKABLE void closeSession();

    Q_INVOKABLE void reloadGroups();
    Q_INVOKABLE bool createGroup(const QString& number);
    Q_INVOKABLE bool deleteGroup(const QString& groupId);

    Q_INVOKABLE void reloadStudents(const QString& groupId);
    Q_INVOKABLE bool addStudent(const QString& groupId, const QString& studentNumber, const QString& fio, const QString& status);
    Q_INVOKABLE bool removeStudent(const QString& studentId);
    Q_INVOKABLE bool updateStudent(const QString& studentId, const QString& fio,
                                   const QString& status, const QString& grade);
    Q_INVOKABLE int importStudents(const QString& groupId, const QString& linesText);

    Q_INVOKABLE void reloadDisciplines();
    Q_INVOKABLE bool addDiscipline(const QString& name);

    Q_INVOKABLE void reloadGradeSheets(const QString& groupId);
    Q_INVOKABLE void reloadPendingSheets();
    Q_INVOKABLE QString createGradeSheet(const QString& groupId, const QString& disciplineId, 
                                        const QString& examType = "exam");
    Q_INVOKABLE QVariantMap gradeSheetInfo(const QString& sheetId) const;
    Q_INVOKABLE QString exportGradeSheetHtml(const QString& sheetId, const QString& baseName) const;
    Q_INVOKABLE bool submitGradeSheetForApproval(const QString& sheetId);
    Q_INVOKABLE bool approveGradeSheet(const QString& sheetId);
    Q_INVOKABLE bool rejectGradeSheet(const QString& sheetId);
    Q_INVOKABLE QVariantList pendingSheetsList() const;

    Q_INVOKABLE void flushSync();

    bool portalSyncBusy() const { return m_portalSyncBusy; }
    QString portalSyncMessage() const { return m_portalSyncMessage; }
    int portalSyncPercent() const { return m_portalSyncPercent; }
    QString portalGroupsIndexUrl() const { return m_portalGroupsIndexUrl; }
    void setPortalGroupsIndexUrl(const QString& url);

    Q_INVOKABLE void syncGroupsFromPortal(const QString& portalLogin, const QString& portalPassword);
    Q_INVOKABLE void importGroupsFromLocalHtml();
    Q_INVOKABLE void openPortalImportFolder();
    Q_INVOKABLE void syncPortalOnlineSavedSession();
    Q_INVOKABLE void clearPortalSession();
    Q_INVOKABLE void schedulePeriodicSync(); // Запланировать периодическую синхронизацию
    Q_INVOKABLE void autoSearchGroups(const QStringList& patterns = {}); // Автопоиск групп

    bool portalHasSavedSession() const;
    bool portalOnlineAutoSync() const { return m_portalOnlineAutoSync; }
    void setPortalOnlineAutoSync(bool enabled);
    QString portalSavedLogin() const { return m_portalSavedLogin; }
    void setPortalSavedLogin(const QString& login);

    QString portalLocalImportDir() const;
    bool portalAutoImport() const { return m_portalAutoImport; }
    void setPortalAutoImport(bool enabled);

signals:
    void sessionOpenChanged();
    void groupsChanged();
    void studentsChanged();
    void disciplinesChanged();
    void gradeSheetsChanged();
    void pendingSheetsChanged();
    void lastErrorChanged();
    void isOnlineChanged();
    void pendingSyncTasksChanged();
    void portalSyncChanged();
    void portalSettingsChanged();
    void portalSyncFinished(int groupsImported, int studentsAdded);

private:
    void tearDownSession();
    void initNetworkMonitoring();
    void setOnline(bool online);
    void setError(const QString& msg);
    void clearError();
    void setupPortalImportWatch();
    void scheduleLocalPortalImport();
    void scheduleOnlinePortalSync();
    void onPeriodicSyncTimer();
    QString portalSessionFilePath() const;

    AcademicRepository* m_repo = nullptr;
    GroupListModel* m_groupsModel = nullptr;
    StudentListModel* m_studentsModel = nullptr;
    DisciplineListModel* m_disciplinesModel = nullptr;
    GradeSheetListModel* m_gradeSheetsModel = nullptr;

    QString m_actorLogin;
    QString m_actorDisplayName;
    QString m_actorRole;
    QString m_lastError;
    QString m_activeGroupId;
    bool m_isOnline = true;

    PortalImportService* m_portalImport = nullptr;
    bool m_portalSyncBusy = false;
    QString m_portalSyncMessage;
    int m_portalSyncPercent = 0;
    QString m_portalGroupsIndexUrl;
    QString m_userDataPath;
    bool m_portalAutoImport = true;
    bool m_portalOnlineAutoSync = false;
    QString m_portalSavedLogin;
    QFileSystemWatcher* m_portalWatcher = nullptr;
    QTimer* m_portalImportDebounce = nullptr;
    QTimer* m_periodicSyncTimer = nullptr;
};

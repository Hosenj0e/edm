#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "../domain/AcademicTypes.h"
#include "../portal/PortalTypes.h"

class AcademicRepository : public QObject
{
    Q_OBJECT

public:
    explicit AcademicRepository(const QString& dbPath, QObject* parent = nullptr);
    ~AcademicRepository() override;

    QVector<StudyGroup> listGroups() const;
    bool createGroup(const QString& number);
    bool deleteGroup(const QString& groupId);
    QString findGroupIdByNumber(const QString& number) const;
    QString upsertGroupFromPortal(const QString& number, const QString& admissionYear, const QString& course,
                                  const QString& specialty, const QString& profile, const QString& institute,
                                  const QString& studyForm, const QString& portalUrl);
    int mergePortalStudents(const QString& groupId, const QList<PortalStudentRow>& students);

    QVector<Student> listStudents(const QString& groupId) const;
    bool addStudent(const QString& groupId, const QString& studentNumber, const QString& fio, const QString& status);
    bool removeStudent(const QString& studentId);
    bool updateStudent(const QString& studentId, const QString& fio, const QString& status, const QString& grade);
    /// Строки вида «номер/ФИО/статус», по одной на строку.
    int importStudentsFromLines(const QString& groupId, const QString& text);

    QVector<Discipline> listDisciplines() const;
    bool addDiscipline(const QString& name);
    bool ensureDefaultDisciplines();

    QVector<GradeSheet> listGradeSheets(const QString& groupId) const;
    QVector<GradeSheet> listPendingSheets() const;
    GradeSheet findGradeSheet(const QString& sheetId) const;
    bool createGradeSheet(const QString& groupId, const QString& disciplineId, 
                         const QString& examType, QString* outSheetId = nullptr);
    bool setGradeSheetStatus(const QString& sheetId, const QString& status);
    bool approveGradeSheet(const QString& sheetId, const QString& approverLogin);

private:
    bool open();
    bool ensureSchema();

    QString m_dbPath;
    QString m_connectionName;
};

#include "AcademicRepository.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

using namespace std;

namespace {
QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString normalizeStatus(const QString& raw)
{
    const QString t = raw.trimmed();
    if (t.isEmpty())
        return QStringLiteral("обучается");
    return t;
}
}

AcademicRepository::AcademicRepository(const QString& dbPath, QObject* parent)
    : QObject(parent)
    , m_dbPath(dbPath)
    , m_connectionName(QStringLiteral("academic-db-") + QString::number(reinterpret_cast<quintptr>(this)))
{
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    open();
    ensureDefaultDisciplines();
}

AcademicRepository::~AcademicRepository()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool AcademicRepository::open()
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(m_dbPath);
    }
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open())
        return false;
    return ensureSchema();
}

bool AcademicRepository::ensureSchema()
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS study_groups ("
            "group_id TEXT PRIMARY KEY NOT NULL,"
            "number TEXT NOT NULL,"
            "created_at INTEGER NOT NULL"
            ");")))
        return false;

    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS students ("
            "student_id TEXT PRIMARY KEY NOT NULL,"
            "group_id TEXT NOT NULL,"
            "student_number TEXT NOT NULL,"
            "fio TEXT NOT NULL,"
            "status TEXT NOT NULL,"
            "grade TEXT NOT NULL DEFAULT '',"
            "FOREIGN KEY(group_id) REFERENCES study_groups(group_id) ON DELETE CASCADE"
            ");")))
        return false;

    // Миграция: добавляем grade если нет
    q.exec(QStringLiteral("ALTER TABLE students ADD COLUMN grade TEXT NOT NULL DEFAULT ''"));

    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS disciplines ("
            "discipline_id TEXT PRIMARY KEY NOT NULL,"
            "name TEXT NOT NULL UNIQUE"
            ");")))
        return false;

    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS grade_sheets ("
            "sheet_id TEXT PRIMARY KEY NOT NULL,"
            "group_id TEXT NOT NULL,"
            "discipline_id TEXT NOT NULL,"
            "title TEXT NOT NULL,"
            "created_at INTEGER NOT NULL,"
            "exam_type TEXT NOT NULL DEFAULT 'exam',"
            "status TEXT NOT NULL DEFAULT 'draft',"
            "approved_by TEXT NOT NULL DEFAULT '',"
            "FOREIGN KEY(group_id) REFERENCES study_groups(group_id) ON DELETE CASCADE,"
            "FOREIGN KEY(discipline_id) REFERENCES disciplines(discipline_id)"
            ");")))
        return false;

    // Миграции для старых БД
    q.exec(QStringLiteral("ALTER TABLE grade_sheets ADD COLUMN status TEXT NOT NULL DEFAULT 'draft'"));
    q.exec(QStringLiteral("ALTER TABLE grade_sheets ADD COLUMN approved_by TEXT NOT NULL DEFAULT ''"));
    q.exec(QStringLiteral("ALTER TABLE grade_sheets ADD COLUMN exam_type TEXT NOT NULL DEFAULT 'exam'"));

    q.exec(QStringLiteral("ALTER TABLE study_groups ADD COLUMN admission_year TEXT NOT NULL DEFAULT ''"));
    q.exec(QStringLiteral("ALTER TABLE study_groups ADD COLUMN course TEXT NOT NULL DEFAULT ''"));
    q.exec(QStringLiteral("ALTER TABLE study_groups ADD COLUMN specialty TEXT NOT NULL DEFAULT ''"));
    q.exec(QStringLiteral("ALTER TABLE study_groups ADD COLUMN profile TEXT NOT NULL DEFAULT ''"));
    q.exec(QStringLiteral("ALTER TABLE study_groups ADD COLUMN institute TEXT NOT NULL DEFAULT ''"));
    q.exec(QStringLiteral("ALTER TABLE study_groups ADD COLUMN study_form TEXT NOT NULL DEFAULT ''"));
    q.exec(QStringLiteral("ALTER TABLE study_groups ADD COLUMN portal_url TEXT NOT NULL DEFAULT ''"));

    return true;
}

bool AcademicRepository::ensureDefaultDisciplines()
{
    if (!listDisciplines().isEmpty())
        return true;

    const QStringList defaults = {
        QStringLiteral("Математика"),
        QStringLiteral("Физика"),
        QStringLiteral("Программирование"),
        QStringLiteral("Иностранный язык"),
    };
    for (const QString& name : defaults)
        addDiscipline(name);
    return true;
}

QVector<StudyGroup> AcademicRepository::listGroups() const
{
    QVector<StudyGroup> out;
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT g.group_id, g.number, g.created_at, "
            "(SELECT COUNT(*) FROM students s WHERE s.group_id = g.group_id), "
            "(SELECT COUNT(*) FROM grade_sheets gs WHERE gs.group_id = g.group_id), "
            "g.admission_year, g.course, g.specialty, g.institute, g.study_form "
            "FROM study_groups g ORDER BY g.number COLLATE NOCASE")))
        return out;

    while (q.next()) {
        StudyGroup g;
        g.id = q.value(0).toString();
        g.number = q.value(1).toString();
        g.createdAt = q.value(2).toLongLong();
        g.studentCount = q.value(3).toInt();
        g.sheetCount = q.value(4).toInt();
        g.admissionYear = q.value(5).toString();
        g.course = q.value(6).toString();
        g.specialty = q.value(7).toString();
        g.institute = q.value(8).toString();
        g.studyForm = q.value(9).toString();

        QStringList subtitleParts;
        if (!g.course.isEmpty())
            subtitleParts << QStringLiteral("%1 курс").arg(g.course);
        if (!g.institute.isEmpty())
            subtitleParts << g.institute;
        if (!g.studyForm.isEmpty())
            subtitleParts << g.studyForm;
        if (!g.specialty.isEmpty() && subtitleParts.size() < 3)
            subtitleParts << g.specialty;
        g.subtitle = subtitleParts.join(QStringLiteral(" · "));

        out.push_back(move(g));
    }
    return out;
}

bool AcademicRepository::createGroup(const QString& number)
{
    const QString n = number.trimmed();
    if (n.isEmpty())
        return false;

    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO study_groups (group_id, number, created_at) VALUES (:id, :num, :ts)"));
    q.bindValue(QStringLiteral(":id"), newId());
    q.bindValue(QStringLiteral(":num"), n);
    q.bindValue(QStringLiteral(":ts"), nowMs());
    return q.exec();
}

QString AcademicRepository::findGroupIdByNumber(const QString& number) const
{
    const QString n = number.trimmed();
    if (n.isEmpty())
        return {};

    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return {};

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT group_id FROM study_groups WHERE number = :num COLLATE NOCASE LIMIT 1"));
    q.bindValue(QStringLiteral(":num"), n);
    if (!q.exec() || !q.next())
        return {};
    return q.value(0).toString();
}

QString AcademicRepository::upsertGroupFromPortal(const QString& number, const QString& admissionYear,
                                                  const QString& course, const QString& specialty,
                                                  const QString& profile, const QString& institute,
                                                  const QString& studyForm, const QString& portalUrl)
{
    const QString n = number.trimmed();
    if (n.isEmpty()) {
        qDebug() << "upsertGroupFromPortal: empty number";
        return {};
    }

    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        qDebug() << "upsertGroupFromPortal: database not open";
        return {};
    }

    const QString existingId = findGroupIdByNumber(n);
    QSqlQuery q(db);
    if (existingId.isEmpty()) {
        const QString newGroupId = newId();
        qDebug() << "Inserting new group:" << n << "with ID:" << newGroupId;
        q.prepare(QStringLiteral(
            "INSERT INTO study_groups (group_id, number, created_at, admission_year, course, specialty, "
            "profile, institute, study_form, portal_url) "
            "VALUES (:id, :num, :ts, :ay, :crs, :spec, :prof, :inst, :form, :url)"));
        q.bindValue(QStringLiteral(":id"), newGroupId);
        q.bindValue(QStringLiteral(":num"), n);
        q.bindValue(QStringLiteral(":ts"), nowMs());
        q.bindValue(QStringLiteral(":ay"), admissionYear.trimmed());
        q.bindValue(QStringLiteral(":crs"), course.trimmed());
        q.bindValue(QStringLiteral(":spec"), specialty.trimmed());
        q.bindValue(QStringLiteral(":prof"), profile.trimmed());
        q.bindValue(QStringLiteral(":inst"), institute.trimmed());
        q.bindValue(QStringLiteral(":form"), studyForm.trimmed());
        q.bindValue(QStringLiteral(":url"), portalUrl.trimmed());
        if (!q.exec()) {
            qDebug() << "Failed to insert group:" << q.lastError().text();
            return {};
        }
        qDebug() << "Group inserted successfully";
        return newGroupId;
    }

    qDebug() << "Updating existing group:" << n << "with ID:" << existingId;
    q.prepare(QStringLiteral(
        "UPDATE study_groups SET admission_year = :ay, course = :crs, specialty = :spec, profile = :prof, "
        "institute = :inst, study_form = :form, portal_url = :url WHERE group_id = :id"));
    q.bindValue(QStringLiteral(":id"), existingId);
    q.bindValue(QStringLiteral(":ay"), admissionYear.trimmed());
    q.bindValue(QStringLiteral(":crs"), course.trimmed());
    q.bindValue(QStringLiteral(":spec"), specialty.trimmed());
    q.bindValue(QStringLiteral(":prof"), profile.trimmed());
    q.bindValue(QStringLiteral(":inst"), institute.trimmed());
    q.bindValue(QStringLiteral(":form"), studyForm.trimmed());
    q.bindValue(QStringLiteral(":url"), portalUrl.trimmed());
    if (!q.exec()) {
        qDebug() << "Failed to update group:" << q.lastError().text();
        return {};
    }
    qDebug() << "Group updated successfully";
    return existingId;
}

int AcademicRepository::mergePortalStudents(const QString& groupId, const QList<PortalStudentRow>& students)
{
    if (groupId.isEmpty())
        return 0;

    int added = 0;
    for (const PortalStudentRow& row : students) {
        const QString num = row.seq.trimmed();
        const QString fio = row.fio.trimmed();
        if (num.isEmpty() || fio.isEmpty())
            continue;

        auto db = QSqlDatabase::database(m_connectionName);
        QSqlQuery find(db);
        find.prepare(QStringLiteral(
            "SELECT student_id FROM students WHERE group_id = :gid AND student_number = :num LIMIT 1"));
        find.bindValue(QStringLiteral(":gid"), groupId);
        find.bindValue(QStringLiteral(":num"), num);
        if (find.exec() && find.next()) {
            QSqlQuery upd(db);
            upd.prepare(QStringLiteral("UPDATE students SET fio = :fio, status = :st WHERE student_id = :id"));
            upd.bindValue(QStringLiteral(":fio"), fio);
            upd.bindValue(QStringLiteral(":st"), normalizeStatus(row.status));
            upd.bindValue(QStringLiteral(":id"), find.value(0).toString());
            upd.exec();
            continue;
        }

        if (addStudent(groupId, num, fio, row.status))
            ++added;
    }
    return added;
}

bool AcademicRepository::deleteGroup(const QString& groupId)
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    if (!db.transaction())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM students WHERE group_id = :gid"));
    q.bindValue(QStringLiteral(":gid"), groupId);
    if (!q.exec()) {
        db.rollback();
        return false;
    }

    q.prepare(QStringLiteral("DELETE FROM grade_sheets WHERE group_id = :gid"));
    q.bindValue(QStringLiteral(":gid"), groupId);
    if (!q.exec()) {
        db.rollback();
        return false;
    }

    q.prepare(QStringLiteral("DELETE FROM study_groups WHERE group_id = :gid"));
    q.bindValue(QStringLiteral(":gid"), groupId);
    if (!q.exec()) {
        db.rollback();
        return false;
    }

    return db.commit();
}

QVector<Student> AcademicRepository::listStudents(const QString& groupId) const
{
    QVector<Student> out;
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT student_id, group_id, student_number, fio, status, grade FROM students "
        "WHERE group_id = :gid ORDER BY student_number COLLATE NOCASE"));
    q.bindValue(QStringLiteral(":gid"), groupId);
    if (!q.exec())
        return out;

    while (q.next()) {
        Student s;
        s.id = q.value(0).toString();
        s.groupId = q.value(1).toString();
        s.studentNumber = q.value(2).toString();
        s.fio = q.value(3).toString();
        s.status = q.value(4).toString();
        s.grade = q.value(5).toString();
        out.push_back(move(s));
    }
    return out;
}

bool AcademicRepository::addStudent(const QString& groupId, const QString& studentNumber, const QString& fio, const QString& status)
{
    const QString num = studentNumber.trimmed();
    const QString name = fio.trimmed();
    if (groupId.isEmpty() || num.isEmpty() || name.isEmpty())
        return false;

    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO students (student_id, group_id, student_number, fio, status) "
        "VALUES (:id, :gid, :num, :fio, :st)"));
    q.bindValue(QStringLiteral(":id"), newId());
    q.bindValue(QStringLiteral(":gid"), groupId);
    q.bindValue(QStringLiteral(":num"), num);
    q.bindValue(QStringLiteral(":fio"), name);
    q.bindValue(QStringLiteral(":st"), normalizeStatus(status));
    return q.exec();
}

bool AcademicRepository::removeStudent(const QString& studentId)
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM students WHERE student_id = :id"));
    q.bindValue(QStringLiteral(":id"), studentId);
    return q.exec();
}

int AcademicRepository::importStudentsFromLines(const QString& groupId, const QString& text)
{
    int added = 0;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;

        const QStringList parts = line.split(QLatin1Char('/'));
        if (parts.size() < 2)
            continue;

        const QString num = parts[0].trimmed();
        const QString fio = parts[1].trimmed();
        const QString status = parts.size() >= 3 ? parts[2].trimmed() : QStringLiteral("обучается");
        if (num.isEmpty() || fio.isEmpty())
            continue;

        if (addStudent(groupId, num, fio, status))
            ++added;
    }
    return added;
}

QVector<Discipline> AcademicRepository::listDisciplines() const
{
    QVector<Discipline> out;
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT discipline_id, name FROM disciplines ORDER BY name COLLATE NOCASE")))
        return out;

    while (q.next()) {
        Discipline d;
        d.id = q.value(0).toString();
        d.name = q.value(1).toString();
        out.push_back(move(d));
    }
    return out;
}

bool AcademicRepository::addDiscipline(const QString& name)
{
    const QString n = name.trimmed();
    if (n.isEmpty())
        return false;

    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO disciplines (discipline_id, name) VALUES (:id, :name)"));
    q.bindValue(QStringLiteral(":id"), newId());
    q.bindValue(QStringLiteral(":name"), n);
    return q.exec();
}

QVector<GradeSheet> AcademicRepository::listGradeSheets(const QString& groupId) const
{
    QVector<GradeSheet> out;
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT gs.sheet_id, gs.group_id, gs.discipline_id, gs.title, gs.created_at, d.name, "
        "gs.exam_type, gs.status, gs.approved_by, sg.number "
        "FROM grade_sheets gs "
        "JOIN disciplines d ON d.discipline_id = gs.discipline_id "
        "JOIN study_groups sg ON sg.group_id = gs.group_id "
        "WHERE gs.group_id = :gid "
        "ORDER BY gs.created_at DESC"));
    q.bindValue(QStringLiteral(":gid"), groupId);
    if (!q.exec())
        return out;

    while (q.next()) {
        GradeSheet sheet;
        sheet.id = q.value(0).toString();
        sheet.groupId = q.value(1).toString();
        sheet.disciplineId = q.value(2).toString();
        sheet.title = q.value(3).toString();
        sheet.createdAt = q.value(4).toLongLong();
        sheet.disciplineName = q.value(5).toString();
        sheet.examType = q.value(6).toString();
        sheet.status = q.value(7).toString();
        sheet.approvedBy = q.value(8).toString();
        sheet.groupNumber = q.value(9).toString();
        out.push_back(move(sheet));
    }
    return out;
}

GradeSheet AcademicRepository::findGradeSheet(const QString& sheetId) const
{
    GradeSheet sheet;
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return sheet;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT gs.sheet_id, gs.group_id, gs.discipline_id, gs.title, gs.created_at, d.name, "
        "gs.exam_type, gs.status, gs.approved_by, sg.number "
        "FROM grade_sheets gs "
        "JOIN disciplines d ON d.discipline_id = gs.discipline_id "
        "JOIN study_groups sg ON sg.group_id = gs.group_id "
        "WHERE gs.sheet_id = :sid"));
    q.bindValue(QStringLiteral(":sid"), sheetId);
    if (!q.exec() || !q.next())
        return sheet;

    sheet.id = q.value(0).toString();
    sheet.groupId = q.value(1).toString();
    sheet.disciplineId = q.value(2).toString();
    sheet.title = q.value(3).toString();
    sheet.createdAt = q.value(4).toLongLong();
    sheet.disciplineName = q.value(5).toString();
    sheet.examType = q.value(6).toString();
    sheet.status = q.value(7).toString();
    sheet.approvedBy = q.value(8).toString();
    sheet.groupNumber = q.value(9).toString();
    return sheet;
}

bool AcademicRepository::createGradeSheet(const QString& groupId, const QString& disciplineId, 
                                         const QString& examType, QString* outSheetId)
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QString groupNumber;
    QString disciplineName;

    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT number FROM study_groups WHERE group_id = :gid"));
        q.bindValue(QStringLiteral(":gid"), groupId);
        if (!q.exec() || !q.next())
            return false;
        groupNumber = q.value(0).toString();
    }
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT name FROM disciplines WHERE discipline_id = :did"));
        q.bindValue(QStringLiteral(":did"), disciplineId);
        if (!q.exec() || !q.next())
            return false;
        disciplineName = q.value(0).toString();
    }

    const QString sheetId = newId();
    const qint64 ts = nowMs();
    const QString typeStr = examType == QStringLiteral("credit") 
        ? QStringLiteral("Зачет") 
        : QStringLiteral("Экзамен");
    const QString title = QStringLiteral("Ведомость (%1): %2 — %3").arg(typeStr, groupNumber, disciplineName);

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO grade_sheets (sheet_id, group_id, discipline_id, title, created_at, exam_type) "
        "VALUES (:sid, :gid, :did, :title, :ts, :type)"));
    q.bindValue(QStringLiteral(":sid"), sheetId);
    q.bindValue(QStringLiteral(":gid"), groupId);
    q.bindValue(QStringLiteral(":did"), disciplineId);
    q.bindValue(QStringLiteral(":title"), title);
    q.bindValue(QStringLiteral(":ts"), ts);
    q.bindValue(QStringLiteral(":type"), examType);
    if (!q.exec())
        return false;

    if (outSheetId)
        *outSheetId = sheetId;
    return true;
}

bool AcademicRepository::updateStudent(const QString& studentId, const QString& fio,
                                       const QString& status, const QString& grade)
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE students SET fio = :fio, status = :st, grade = :grade WHERE student_id = :id"));
    q.bindValue(QStringLiteral(":fio"), fio.trimmed());
    q.bindValue(QStringLiteral(":st"), normalizeStatus(status));
    q.bindValue(QStringLiteral(":grade"), grade.trimmed());
    q.bindValue(QStringLiteral(":id"), studentId);
    return q.exec();
}

bool AcademicRepository::setGradeSheetStatus(const QString& sheetId, const QString& status)
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    // Добавляем колонку status если её нет (миграция)
    QSqlQuery mig(db);
    mig.exec(QStringLiteral("ALTER TABLE grade_sheets ADD COLUMN status TEXT NOT NULL DEFAULT 'draft'"));

    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE grade_sheets SET status = :st WHERE sheet_id = :sid"));
    q.bindValue(QStringLiteral(":st"), status);
    q.bindValue(QStringLiteral(":sid"), sheetId);
    return q.exec();
}

QVector<GradeSheet> AcademicRepository::listPendingSheets() const
{
    QVector<GradeSheet> out;
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT gs.sheet_id, gs.group_id, gs.discipline_id, gs.title, gs.created_at, d.name, "
            "gs.exam_type, gs.status, gs.approved_by, sg.number "
            "FROM grade_sheets gs "
            "JOIN disciplines d ON d.discipline_id = gs.discipline_id "
            "JOIN study_groups sg ON sg.group_id = gs.group_id "
            "WHERE gs.status = 'pending_approval' "
            "ORDER BY gs.created_at DESC")))
        return out;

    while (q.next()) {
        GradeSheet sheet;
        sheet.id = q.value(0).toString();
        sheet.groupId = q.value(1).toString();
        sheet.disciplineId = q.value(2).toString();
        sheet.title = q.value(3).toString();
        sheet.createdAt = q.value(4).toLongLong();
        sheet.disciplineName = q.value(5).toString();
        sheet.examType = q.value(6).toString();
        sheet.status = q.value(7).toString();
        sheet.approvedBy = q.value(8).toString();
        sheet.groupNumber = q.value(9).toString();
        out.push_back(move(sheet));
    }
    return out;
}

bool AcademicRepository::approveGradeSheet(const QString& sheetId, const QString& approverLogin)
{
    auto db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE grade_sheets SET status = 'approved', approved_by = :approver "
        "WHERE sheet_id = :sid AND status = 'pending_approval'"));
    q.bindValue(QStringLiteral(":approver"), approverLogin);
    q.bindValue(QStringLiteral(":sid"), sheetId);
    return q.exec() && q.numRowsAffected() > 0;
}

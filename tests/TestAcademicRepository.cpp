#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "storage/AcademicRepository.h"
#include "domain/AcademicTypes.h"
#include "portal/PortalTypes.h"

class TestAcademicRepository final : public QObject
{
    Q_OBJECT

private slots:
    void testCreateGroup_success()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("academic.sqlite");
        AcademicRepository repo(dbPath);

        const bool success = repo.createGroup("2991");
        QVERIFY(success);
        
        const QVector<StudyGroup> groups = repo.listGroups();
        QVERIFY(groups.size() >= 1);
        
        bool found = false;
        for (const auto& g : groups) {
            if (g.number == "2991") {
                found = true;
                QCOMPARE(g.number, QStringLiteral("2991"));
            }
        }
        QVERIFY(found);
    }

    void testAddStudent_toGroup()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("academic.sqlite");
        AcademicRepository repo(dbPath);

        QVERIFY(repo.createGroup("2991"));
        const QString groupId = repo.findGroupIdByNumber("2991");
        QVERIFY(!groupId.isEmpty());

        const bool success = repo.addStudent(groupId, "1", "Иванов Иван Иванович", "обучается");
        QVERIFY(success);

        const QVector<Student> students = repo.listStudents(groupId);
        QCOMPARE(students.size(), 1);
        QCOMPARE(students[0].fio, QStringLiteral("Иванов Иван Иванович"));
        QCOMPARE(students[0].status, QStringLiteral("обучается"));
    }

    void testUpdateStudent_grade()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("academic.sqlite");
        AcademicRepository repo(dbPath);

        QVERIFY(repo.createGroup("2991"));
        const QString groupId = repo.findGroupIdByNumber("2991");
        QVERIFY(repo.addStudent(groupId, "1", "Иванов И.И.", "обучается"));
        
        const QVector<Student> students = repo.listStudents(groupId);
        QCOMPARE(students.size(), 1);
        const QString studentId = students[0].id;

        // Обновляем оценку
        QVERIFY(repo.updateStudent(studentId, "Иванов И.И.", "обучается", "5"));

        const QVector<Student> updatedStudents = repo.listStudents(groupId);
        QCOMPARE(updatedStudents.size(), 1);
        QCOMPARE(updatedStudents[0].grade, QStringLiteral("5"));
    }

    void testDeleteStudent_success()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("academic.sqlite");
        AcademicRepository repo(dbPath);

        QVERIFY(repo.createGroup("2991"));
        const QString groupId = repo.findGroupIdByNumber("2991");
        QVERIFY(repo.addStudent(groupId, "1", "Иванов И.И.", "обучается"));

        QVector<Student> students = repo.listStudents(groupId);
        QCOMPARE(students.size(), 1);
        const QString studentId = students[0].id;

        QVERIFY(repo.removeStudent(studentId));
        QCOMPARE(repo.listStudents(groupId).size(), 0);
    }

    void testCreateGradeSheet_success()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("academic.sqlite");
        AcademicRepository repo(dbPath);

        QVERIFY(repo.createGroup("2991"));
        const QString groupId = repo.findGroupIdByNumber("2991");
        QVERIFY(!groupId.isEmpty());
        
        // Добавляем дисциплину
        QVERIFY(repo.addDiscipline("Математика"));
        const QVector<Discipline> disciplines = repo.listDisciplines();
        QVERIFY(!disciplines.isEmpty());
        QString disciplineId;
        for (const auto& d : disciplines) {
            if (d.name == "Математика") {
                disciplineId = d.id;
                break;
            }
        }
        QVERIFY(!disciplineId.isEmpty());

        QString sheetId;
        QVERIFY(repo.createGradeSheet(groupId, disciplineId, "exam", &sheetId));
        QVERIFY(!sheetId.isEmpty());

        const QVector<GradeSheet> sheets = repo.listGradeSheets(groupId);
        QCOMPARE(sheets.size(), 1);
        QCOMPARE(sheets[0].examType, QStringLiteral("exam"));
    }

    void testDeleteGroup_cascade()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("academic.sqlite");
        AcademicRepository repo(dbPath);

        QVERIFY(repo.createGroup("2991"));
        const QString groupId = repo.findGroupIdByNumber("2991");
        repo.addStudent(groupId, "1", "Иванов И.И.", "обучается");

        QVERIFY(repo.deleteGroup(groupId));

        QCOMPARE(repo.listGroups().size(), 0);
        QCOMPARE(repo.listStudents(groupId).size(), 0);
        QCOMPARE(repo.listGradeSheets(groupId).size(), 0);
    }

    void testUpsertGroupFromPortal_createsOrUpdates()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("academic.sqlite");
        AcademicRepository repo(dbPath);

        // Первый вызов - создание
        const QString groupId1 = repo.upsertGroupFromPortal(
            "2991", "2022", "4", "ИСиП", "Разработка ПО", "ПИ", "очная", "https://portal.novsu.ru/group/2991"
        );
        QVERIFY(!groupId1.isEmpty());

        // Второй вызов с тем же номером - обновление
        const QString groupId2 = repo.upsertGroupFromPortal(
            "2991", "2022", "5", "ИСиП", "Разработка ПО", "ПИ", "очная", "https://portal.novsu.ru/group/2991"
        );
        
        QCOMPARE(groupId1, groupId2); // Тот же ID
        QCOMPARE(repo.listGroups().size(), 1); // Одна группа
    }

    void testMergePortalStudents_addsNew()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("academic.sqlite");
        AcademicRepository repo(dbPath);

        QVERIFY(repo.createGroup("2991"));
        const QString groupId = repo.findGroupIdByNumber("2991");
        
        QList<PortalStudentRow> portalStudents;
        portalStudents.append(PortalStudentRow{"1", "Иванов И.И.", "обучается"});
        portalStudents.append(PortalStudentRow{"2", "Петров П.П.", "обучается"});

        const int added = repo.mergePortalStudents(groupId, portalStudents);
        QCOMPARE(added, 2);

        const QVector<Student> students = repo.listStudents(groupId);
        QCOMPARE(students.size(), 2);
    }

    void testMergePortalStudents_updatesExisting()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath("academic.sqlite");
        AcademicRepository repo(dbPath);

        QVERIFY(repo.createGroup("2991"));
        const QString groupId = repo.findGroupIdByNumber("2991");
        repo.addStudent(groupId, "1", "Иванов Иван", "обучается");

        QList<PortalStudentRow> portalStudents;
        portalStudents.append(PortalStudentRow{"1", "Иванов Иван Иванович", "академический отпуск"});

        const int added = repo.mergePortalStudents(groupId, portalStudents);
        QVERIFY(added >= 0); // Может обновить существующего

        const QVector<Student> students = repo.listStudents(groupId);
        QCOMPARE(students.size(), 1);
        QCOMPARE(students[0].fio, QStringLiteral("Иванов Иван Иванович"));
        QCOMPARE(students[0].status, QStringLiteral("академический отпуск"));
    }
};

QTEST_MAIN(TestAcademicRepository)
#include "TestAcademicRepository.moc"

#pragma once

#include <QString>

struct StudyGroup {
    QString id;
    QString number; // номер группы, напр. 2991
    qint64 createdAt = 0;
    int studentCount = 0;
    int sheetCount = 0;
    QString admissionYear;
    QString course;
    QString specialty;
    QString institute;
    QString studyForm;
    QString subtitle; // для карточки на главном экране
};

struct Student {
    QString id;
    QString groupId;
    QString studentNumber;
    QString fio;
    QString status;
    QString grade;   // оценка: "2","3","4","5" или ""
};

struct Discipline {
    QString id;
    QString name;
};

struct GradeSheet {
    QString id;
    QString groupId;
    QString groupNumber;
    QString disciplineId;
    QString title;
    QString disciplineName;
    QString examType;    // "exam" | "credit" (экзамен | зачет)
    QString status;      // "draft" | "pending_approval" | "approved"
    QString approvedBy;
    qint64 createdAt = 0;
};

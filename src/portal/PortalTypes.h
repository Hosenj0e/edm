#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QUrl>

struct PortalStudentRow {
    QString seq;
    QString fio;
    QString status;
};

struct PortalGroupDetail {
    QString number;
    QString admissionYear;
    QString course;
    QString specialty;
    QString profile;
    QString institute;
    QString studyForm;
    QString sourceUrl;
    QList<PortalStudentRow> students;
    
    // Проверка, является ли группа активной в текущем семестре
    bool isActiveInCurrentSemester() const;
    
    // Проверка, есть ли активные студенты
    bool hasActiveStudents() const;
};

struct PortalLoginForm {
    QUrl action;
    QString method = QStringLiteral("POST");
    QHash<QString, QString> fields;
    bool valid = false;
};


#include "PortalTypes.h"
#include <QDate>

bool PortalGroupDetail::isActiveInCurrentSemester() const
{
    // Получаем текущую дату
    const QDate today = QDate::currentDate();
    const int currentYear = today.year();
    const int currentMonth = today.month();
    
    // Определяем текущий учебный год (сентябрь-август)
    int academicYear = currentYear;
    if (currentMonth < 9) {
        academicYear = currentYear - 1;
    }
    
    // Парсим год поступления
    bool ok = false;
    int admYear = admissionYear.toInt(&ok);
    if (!ok || admYear < 2000 || admYear > currentYear + 1) {
        // Если год поступления не указан или некорректен, пробуем определить по курсу
        if (!course.isEmpty()) {
            int courseNum = course.left(1).toInt(&ok);
            if (ok && courseNum >= 1 && courseNum <= 6) {
                // Считаем, что группа активна, если курс от 1 до 4 (бакалавриат/специалитет)
                // или 1-2 (магистратура)
                return courseNum <= 4;
            }
        }
        // Если ничего не известно, считаем группу активной
        return true;
    }
    
    // Вычисляем, сколько лет прошло с момента поступления
    int yearsStudying = academicYear - admYear;
    
    // Проверяем форму обучения
    const bool isFullTime = studyForm.contains(QStringLiteral("очн"), Qt::CaseInsensitive);
    const bool isPartTime = studyForm.contains(QStringLiteral("заочн"), Qt::CaseInsensitive);
    
    // Для очной формы: бакалавриат 4 года, специалитет 5 лет, магистратура 2 года
    // Для заочной: +1 год
    int maxYears = 5; // по умолчанию специалитет
    
    if (specialty.contains(QStringLiteral("бакалавр"), Qt::CaseInsensitive)) {
        maxYears = isPartTime ? 5 : 4;
    } else if (specialty.contains(QStringLiteral("магистр"), Qt::CaseInsensitive)) {
        maxYears = isPartTime ? 3 : 2;
    } else if (specialty.contains(QStringLiteral("специалитет"), Qt::CaseInsensitive)) {
        maxYears = isPartTime ? 6 : 5;
    }
    
    // Группа активна, если не превышен срок обучения
    return yearsStudying >= 0 && yearsStudying < maxYears;
}

bool PortalGroupDetail::hasActiveStudents() const
{
    for (const auto& student : students) {
        if (student.status.contains(QStringLiteral("обучается"), Qt::CaseInsensitive) ||
            student.status.contains(QStringLiteral("СТ"), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

#include "StudentListModel.h"

StudentListModel::StudentListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int StudentListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant StudentListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const auto& s = m_items[index.row()];
    switch (role) {
    case IdRole:
        return s.id;
    case GroupIdRole:
        return s.groupId;
    case StudentNumberRole:
        return s.studentNumber;
    case FioRole:
        return s.fio;
    case StatusRole:
        return s.status;
    case GradeRole:
        return s.grade;
    default:
        return {};
    }
}

QHash<int, QByteArray> StudentListModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("studentId")},
        {GroupIdRole, QByteArrayLiteral("groupId")},
        {StudentNumberRole, QByteArrayLiteral("studentNumber")},
        {FioRole, QByteArrayLiteral("fio")},
        {StatusRole, QByteArrayLiteral("status")},
        {GradeRole, QByteArrayLiteral("grade")}
    };
}

void StudentListModel::setStudents(const QVector<Student>& students)
{
    beginResetModel();
    m_items = students;
    endResetModel();
}

int StudentListModel::count() const
{
    return m_items.size();
}

QVariantMap StudentListModel::itemAt(int row) const
{
    QVariantMap m;
    if (row < 0 || row >= m_items.size())
        return m;
    const auto& s = m_items[row];
    m.insert(QStringLiteral("studentId"), s.id);
    m.insert(QStringLiteral("groupId"), s.groupId);
    m.insert(QStringLiteral("studentNumber"), s.studentNumber);
    m.insert(QStringLiteral("fio"), s.fio);
    m.insert(QStringLiteral("status"), s.status);
    m.insert(QStringLiteral("grade"), s.grade);
    return m;
}

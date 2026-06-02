#include "GroupListModel.h"

GroupListModel::GroupListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int GroupListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant GroupListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const auto& g = m_items[index.row()];
    switch (role) {
    case IdRole:
        return g.id;
    case NumberRole:
        return g.number;
    case CreatedAtRole:
        return g.createdAt;
    case StudentCountRole:
        return g.studentCount;
    case SheetCountRole:
        return g.sheetCount;
    case SubtitleRole:
        return g.subtitle;
    case SpecialtyRole:
        return g.specialty;
    case CourseRole:
        return g.course;
    default:
        return {};
    }
}

QHash<int, QByteArray> GroupListModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("groupId")},
        {NumberRole, QByteArrayLiteral("number")},
        {CreatedAtRole, QByteArrayLiteral("createdAt")},
        {StudentCountRole, QByteArrayLiteral("studentCount")},
        {SheetCountRole, QByteArrayLiteral("sheetCount")},
        {SubtitleRole, QByteArrayLiteral("subtitle")},
        {SpecialtyRole, QByteArrayLiteral("specialty")},
        {CourseRole, QByteArrayLiteral("course")}
    };
}

void GroupListModel::setGroups(const QVector<StudyGroup>& groups)
{
    beginResetModel();
    m_items = groups;
    endResetModel();
}

int GroupListModel::count() const
{
    return m_items.size();
}

QVariantMap GroupListModel::itemAt(int row) const
{
    QVariantMap m;
    if (row < 0 || row >= m_items.size())
        return m;
    const auto& g = m_items[row];
    m.insert(QStringLiteral("groupId"), g.id);
    m.insert(QStringLiteral("number"), g.number);
    m.insert(QStringLiteral("createdAt"), g.createdAt);
    m.insert(QStringLiteral("studentCount"), g.studentCount);
    m.insert(QStringLiteral("sheetCount"), g.sheetCount);
    m.insert(QStringLiteral("subtitle"), g.subtitle);
    m.insert(QStringLiteral("specialty"), g.specialty);
    m.insert(QStringLiteral("course"), g.course);
    return m;
}

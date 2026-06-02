#include "GradeSheetListModel.h"

GradeSheetListModel::GradeSheetListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int GradeSheetListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant GradeSheetListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const auto& s = m_items[index.row()];
    switch (role) {
    case IdRole:
        return s.id;
    case GroupIdRole:
        return s.groupId;
    case DisciplineIdRole:
        return s.disciplineId;
    case TitleRole:
        return s.title;
    case DisciplineNameRole:
        return s.disciplineName;
    case CreatedAtRole:
        return s.createdAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> GradeSheetListModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("sheetId")},
        {GroupIdRole, QByteArrayLiteral("groupId")},
        {DisciplineIdRole, QByteArrayLiteral("disciplineId")},
        {TitleRole, QByteArrayLiteral("title")},
        {DisciplineNameRole, QByteArrayLiteral("disciplineName")},
        {CreatedAtRole, QByteArrayLiteral("createdAt")}
    };
}

void GradeSheetListModel::setSheets(const QVector<GradeSheet>& sheets)
{
    beginResetModel();
    m_items = sheets;
    endResetModel();
}

int GradeSheetListModel::count() const
{
    return m_items.size();
}

QVariantMap GradeSheetListModel::itemAt(int row) const
{
    QVariantMap m;
    if (row < 0 || row >= m_items.size())
        return m;
    const auto& s = m_items[row];
    m.insert(QStringLiteral("sheetId"), s.id);
    m.insert(QStringLiteral("groupId"), s.groupId);
    m.insert(QStringLiteral("disciplineId"), s.disciplineId);
    m.insert(QStringLiteral("title"), s.title);
    m.insert(QStringLiteral("disciplineName"), s.disciplineName);
    m.insert(QStringLiteral("createdAt"), s.createdAt);
    return m;
}

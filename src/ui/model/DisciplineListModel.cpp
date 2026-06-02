#include "DisciplineListModel.h"

DisciplineListModel::DisciplineListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int DisciplineListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant DisciplineListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const auto& d = m_items[index.row()];
    switch (role) {
    case IdRole:
        return d.id;
    case NameRole:
        return d.name;
    default:
        return {};
    }
}

QHash<int, QByteArray> DisciplineListModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("disciplineId")},
        {NameRole, QByteArrayLiteral("name")}
    };
}

void DisciplineListModel::setDisciplines(const QVector<Discipline>& disciplines)
{
    beginResetModel();
    m_items = disciplines;
    endResetModel();
}

int DisciplineListModel::count() const
{
    return m_items.size();
}

QVariantMap DisciplineListModel::itemAt(int row) const
{
    QVariantMap m;
    if (row < 0 || row >= m_items.size())
        return m;
    const auto& d = m_items[row];
    m.insert(QStringLiteral("disciplineId"), d.id);
    m.insert(QStringLiteral("name"), d.name);
    return m;
}

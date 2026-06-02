#include "DocumentListModel.h"

#include <QVariant>

DocumentListModel::DocumentListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

DocumentListModel::~DocumentListModel() = default;

int DocumentListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant DocumentListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    const auto row = index.row();
    if (row < 0 || row >= m_items.size())
        return {};

    const auto &item = m_items[row];
    switch (role) {
    case IdRole:
        return item.id;
    case TitleRole:
        return item.title;
    case StatusRole:
        return item.status;
    case VersionRole:
        return item.version;
    case UpdatedAtRole:
        return item.updatedAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> DocumentListModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("docId")},
        {TitleRole, QByteArrayLiteral("title")},
        {StatusRole, QByteArrayLiteral("status")},
        {VersionRole, QByteArrayLiteral("version")},
        {UpdatedAtRole, QByteArrayLiteral("updatedAt")}
    };
}

void DocumentListModel::setDocuments(const QVector<DocumentUiItem>& docs)
{
    beginResetModel();
    m_items = docs;
    endResetModel();
}

int DocumentListModel::documentCount() const
{
    return m_items.size();
}

QVariantMap DocumentListModel::itemAt(int row) const
{
    QVariantMap out;
    if (row < 0 || row >= m_items.size())
        return out;

    const auto& item = m_items[row];
    out.insert(QStringLiteral("docId"), item.id);
    out.insert(QStringLiteral("title"), item.title);
    out.insert(QStringLiteral("status"), item.status);
    out.insert(QStringLiteral("version"), item.version);
    out.insert(QStringLiteral("updatedAt"), item.updatedAt);
    return out;
}


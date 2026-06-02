#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QString>
#include <QVariantMap>

class DocumentListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        StatusRole,
        VersionRole,
        UpdatedAtRole
    };
    Q_ENUM(Role)

    explicit DocumentListModel(QObject* parent = nullptr);
    ~DocumentListModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    struct DocumentUiItem {
        QString id;
        QString title;
        int status = 0;
        int version = 0;
        qint64 updatedAt = 0;
    };

    void setDocuments(const QVector<DocumentUiItem>& docs);

    Q_INVOKABLE int documentCount() const;
    Q_INVOKABLE QVariantMap itemAt(int row) const;

private:
    QVector<DocumentUiItem> m_items;
};


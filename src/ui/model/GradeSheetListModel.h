#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QVariantMap>

#include "../../domain/AcademicTypes.h"

class GradeSheetListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        GroupIdRole,
        DisciplineIdRole,
        TitleRole,
        DisciplineNameRole,
        CreatedAtRole
    };
    Q_ENUM(Role)

    explicit GradeSheetListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setSheets(const QVector<GradeSheet>& sheets);

    Q_INVOKABLE int count() const;
    Q_INVOKABLE QVariantMap itemAt(int row) const;

private:
    QVector<GradeSheet> m_items;
};

#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QVariantMap>

#include "../../domain/AcademicTypes.h"

class DisciplineListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole
    };
    Q_ENUM(Role)

    explicit DisciplineListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDisciplines(const QVector<Discipline>& disciplines);

    Q_INVOKABLE int count() const;
    Q_INVOKABLE QVariantMap itemAt(int row) const;

private:
    QVector<Discipline> m_items;
};

#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QVariantMap>

#include "../../domain/AcademicTypes.h"

class StudentListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        GroupIdRole,
        StudentNumberRole,
        FioRole,
        StatusRole,
        GradeRole
    };
    Q_ENUM(Role)

    explicit StudentListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setStudents(const QVector<Student>& students);

    Q_INVOKABLE int count() const;
    Q_INVOKABLE QVariantMap itemAt(int row) const;

private:
    QVector<Student> m_items;
};

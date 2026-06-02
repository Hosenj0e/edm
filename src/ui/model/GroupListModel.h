#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QVariantMap>

#include "../../domain/AcademicTypes.h"

class GroupListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NumberRole,
        CreatedAtRole,
        StudentCountRole,
        SheetCountRole,
        SubtitleRole,
        SpecialtyRole,
        CourseRole
    };
    Q_ENUM(Role)

    explicit GroupListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setGroups(const QVector<StudyGroup>& groups);

    Q_INVOKABLE int count() const;
    Q_INVOKABLE QVariantMap itemAt(int row) const;

private:
    QVector<StudyGroup> m_items;
};

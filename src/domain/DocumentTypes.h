#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>

enum class DocumentStatus : int {
    Draft = 0,
    InApproval = 1,
    Approved = 2,
    Signed = 3
};

struct ApprovalStep {
    int stepIndex = 0;
    QString approverId;
    int status = 0; // 0 - pending, 1 - approved
    qint64 updatedAt = 0;
};

struct SignatureRecord {
    QString signatureId;
    QString signerId;
    QByteArray signature;
    qint64 signedAt = 0;
};

struct DocumentSummary {
    QString id;
    QString title;
    DocumentStatus status = DocumentStatus::Draft;
    int version = 0;
    qint64 updatedAt = 0;
};


#pragma once

#include <libpq-fe.h>

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class PgDatabase {
public:
    struct Row {
        QHash<QString, QString> fields;
        QString value(const QString& name) const { return fields.value(name); }
    };

    explicit PgDatabase(const QString& conninfo);
    ~PgDatabase();

    PgDatabase(const PgDatabase&) = delete;
    PgDatabase& operator=(const PgDatabase&) = delete;

    QVector<Row> query(const QString& sql, const QStringList& params = {});
    void exec(const QString& sql, const QStringList& params = {});
    bool connected() const;

private:
    PGconn* connection_ = nullptr;
    PGresult* run(const QString& sql, const QStringList& params);
};

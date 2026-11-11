#include "pg_database.hpp"

#include <QByteArray>

#include <stdexcept>
#include <vector>

PgDatabase::PgDatabase(const QString& conninfo) {
    const QByteArray encoded = conninfo.toUtf8();
    connection_ = PQconnectdb(encoded.constData());
    if (!connected()) {
        const QString message = QString::fromUtf8(PQerrorMessage(connection_));
        if (connection_) PQfinish(connection_);
        connection_ = nullptr;
        throw std::runtime_error(message.toStdString());
    }
}

PgDatabase::~PgDatabase() {
    if (connection_) PQfinish(connection_);
}

bool PgDatabase::connected() const {
    return connection_ && PQstatus(connection_) == CONNECTION_OK;
}

PGresult* PgDatabase::run(const QString& sql, const QStringList& params) {
    const QByteArray encodedSql = sql.toUtf8();
    std::vector<QByteArray> values;
    std::vector<const char*> pointers;
    values.reserve(static_cast<size_t>(params.size()));
    pointers.reserve(static_cast<size_t>(params.size()));
    for (const QString& param : params) values.push_back(param.toUtf8());
    for (const QByteArray& value : values) pointers.push_back(value.constData());

    PGresult* result = PQexecParams(connection_, encodedSql.constData(),
                                    static_cast<int>(pointers.size()), nullptr,
                                    pointers.empty() ? nullptr : pointers.data(),
                                    nullptr, nullptr, 0);
    if (!result) throw std::runtime_error(PQerrorMessage(connection_));

    const ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        const QString message = QString::fromUtf8(PQresultErrorMessage(result));
        PQclear(result);
        throw std::runtime_error(message.toStdString());
    }
    return result;
}

QVector<PgDatabase::Row> PgDatabase::query(const QString& sql,
                                            const QStringList& params) {
    PGresult* result = run(sql, params);
    QVector<Row> rows;
    const int count = PQntuples(result);
    const int columns = PQnfields(result);
    rows.reserve(count);
    for (int rowIndex = 0; rowIndex < count; ++rowIndex) {
        Row row;
        for (int column = 0; column < columns; ++column) {
            const QString name = QString::fromUtf8(PQfname(result, column));
            const QString value = PQgetisnull(result, rowIndex, column)
                ? QString()
                : QString::fromUtf8(PQgetvalue(result, rowIndex, column));
            row.fields.insert(name, value);
        }
        rows.push_back(std::move(row));
    }
    PQclear(result);
    return rows;
}

void PgDatabase::exec(const QString& sql, const QStringList& params) {
    PGresult* result = run(sql, params);
    PQclear(result);
}

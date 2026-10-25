#!/usr/bin/env bash
set -eu
. ./.env

export PGHOST=127.0.0.1 PGPORT="$REPLICA_PORT" PGUSER="$ANALYST_USER" PGDATABASE="$POSTGRES_DB" PGPASSWORD="$ANALYST_PASSWORD"

psql -c "SELECT pg_is_in_recovery(), current_user, count(*) FROM app.replication_probe"
psql -c "INSERT INTO app.replication_probe (marker) VALUES ('analytics')" && { echo "запись на реплике неожиданно прошла"; exit 1; } || echo "запись отклонена, как и ожидалось"
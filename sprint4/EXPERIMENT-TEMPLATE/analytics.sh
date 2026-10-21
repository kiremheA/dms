#!/usr/bin/env bash
set -a
. ./.env
set +a

export PGHOST=127.0.0.1 PGPORT="$REPLICA_PORT" PGUSER="$ANALYST_USER" PGDATABASE="$POSTGRES_DB" PGPASSWORD="$ANALYST_PASSWORD"

psql -c "SELECT pg_is_in_recovery(), inet_server_port(), count(*) FROM app.replication_probe"
psql -c "INSERT INTO app.replication_probe (marker) VALUES ('analytics')"
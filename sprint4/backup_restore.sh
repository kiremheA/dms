#!/usr/bin/env bash
set -eu

DB=$1

pg_dumpall --roles-only -f /tmp/roles.sql
pg_dump -Fc -d "$DB" -f "/tmp/$DB.dump"

initdb -U postgres -D /tmp/restore_pgdata
pg_ctl -D /tmp/restore_pgdata -o "-p 5440" -l /tmp/restore_pg.log start

psql -p 5440 -U postgres -d postgres -f /tmp/roles.sql
createdb -p 5440 -U postgres "$DB"
pg_restore -p 5440 -U postgres -d "$DB" "/tmp/$DB.dump"
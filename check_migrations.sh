#!/bin/bash
set -eo pipefail

DIR=${1:-.}
if [ -n "$2" ]; then export PGDATABASE=$2; fi
PSQL="psql -X -q -At -v ON_ERROR_STOP=1"

fail() { echo "Ошибка: $1" >&2; exit 1; }

SNAP="SELECT concat_ws('|', 'T', relname, relkind) FROM pg_class
WHERE relnamespace = 'public'::regnamespace AND relkind IN ('r', 'S')
UNION ALL
SELECT concat_ws('|', 'C', c.relname, a.attnum, a.attname, format_type(a.atttypid, a.atttypmod), a.attnotnull, a.attidentity, pg_get_expr(d.adbin, d.adrelid))
FROM pg_attribute a
JOIN pg_class c ON c.oid = a.attrelid
LEFT JOIN pg_attrdef d ON d.adrelid = a.attrelid AND d.adnum = a.attnum
WHERE c.relnamespace = 'public'::regnamespace AND c.relkind = 'r' AND a.attnum > 0 AND NOT a.attisdropped
UNION ALL
SELECT concat_ws('|', 'K', conrelid::regclass, conname, pg_get_constraintdef(oid))
FROM pg_constraint WHERE connamespace = 'public'::regnamespace
UNION ALL
SELECT concat_ws('|', 'I', indexname, indexdef) FROM pg_indexes WHERE schemaname = 'public';"

run() {
  {
    echo "BEGIN;"
    echo "$1" | while read -r f; do echo "\i '$f'"; done
    echo "$SNAP"
    echo "ROLLBACK;"
  } | $PSQL | sort
}

files=$(ls "$DIR"/migrations/[0-9]*_*.sql 2>/dev/null | sort -V) || fail "нет миграций"
count=$(echo "$files" | wc -l)
if [ "$count" -lt 2 ]; then fail "меньше двух миграций"; fi

tables=$($PSQL -c "SELECT count(*) FROM pg_tables WHERE schemaname = 'public'") || fail "нет доступа к базе"
if [ "$tables" != 0 ]; then fail "база не пустая"; fi

first=$(echo "$files" | head -n $((count - 1)))
last=$(echo "$files" | tail -n 1)
up=$(
  echo "$first"
  if [ -f "$DIR/upgrade_seed.sql" ]; then echo "$DIR/upgrade_seed.sql"; fi
  echo "$last"
)

clean=$(run "$files") || fail "ошибка на чистой базе"
upgrade=$(run "$up") || fail "ошибка при обновлении"
if [ "$clean" != "$upgrade" ]; then fail "схемы не совпадают"; fi

echo OK
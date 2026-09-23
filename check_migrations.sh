#!/usr/bin/env bash
set -euo pipefail

DIR=${1:-.}
M="$DIR/migrations"
P="mig_check_$$"
MIG=("$M"/[0-9]*_*.sql)
N=${#MIG[@]}

cleanup() { for s in clean upgrade; do dropdb --if-exists "${P}_$s" >/dev/null 2>&1 || true; done; }
trap cleanup EXIT
fail() { echo "FAIL: $*"; exit 1; }
newdb() { dropdb --if-exists "$1" >/dev/null 2>&1 || true; createdb "$1"; }
apply() { local db=$1; shift; for f in "$@"; do psql -X -q -v ON_ERROR_STOP=1 -d "$db" -f "$f" >/dev/null || fail "$(basename "$f") -> $db"; done; }

[ "$N" -ge 2 ] && [ -f "${MIG[0]}" ] || fail "нужно минимум две миграции в $M"

schema() { psql -X -At -v ON_ERROR_STOP=1 -d "$1" <<'SQL' | LC_ALL=C sort
SELECT 'T|'||n.nspname||'|'||c.relname||'|'||c.relkind::text
FROM pg_class c JOIN pg_namespace n ON n.oid=c.relnamespace
WHERE n.nspname='public' AND c.relkind IN ('r','S')
UNION ALL
SELECT 'C|'||n.nspname||'|'||c.relname||'|'||a.attnum||'|'||a.attname||'|'||format_type(a.atttypid,a.atttypmod)||'|'||a.attnotnull||'|'||a.attidentity::text||'|'||a.attgenerated::text||'|'||COALESCE(pg_get_expr(d.adbin,d.adrelid),'')
FROM pg_attribute a JOIN pg_class c ON c.oid=a.attrelid JOIN pg_namespace n ON n.oid=c.relnamespace
LEFT JOIN pg_attrdef d ON d.adrelid=a.attrelid AND d.adnum=a.attnum
WHERE n.nspname='public' AND c.relkind='r' AND a.attnum>0 AND NOT a.attisdropped
UNION ALL
SELECT 'K|'||n.nspname||'|'||c.relname||'|'||con.conname||'|'||con.contype::text||'|'||pg_get_constraintdef(con.oid,true)
FROM pg_constraint con JOIN pg_class c ON c.oid=con.conrelid JOIN pg_namespace n ON n.oid=c.relnamespace
WHERE n.nspname='public' AND c.relkind='r'
UNION ALL
SELECT 'I|'||n.nspname||'|'||c.relname||'|'||i.relname||'|'||pg_get_indexdef(i.oid)
FROM pg_index x JOIN pg_class c ON c.oid=x.indrelid JOIN pg_class i ON i.oid=x.indexrelid JOIN pg_namespace n ON n.oid=c.relnamespace
WHERE n.nspname='public' AND c.relkind='r';
SQL
}

newdb "${P}_clean"
apply "${P}_clean" "${MIG[@]}"

newdb "${P}_upgrade"
apply "${P}_upgrade" "${MIG[@]:0:$((N-1))}"
apply "${P}_upgrade" "${MIG[$((N-1))]}"

[ "$(schema "${P}_clean")" = "$(schema "${P}_upgrade")" ] || fail "обновление с предыдущей версии: схема отличается от чистой"

echo OK
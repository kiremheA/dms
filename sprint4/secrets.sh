#!/usr/bin/env bash

set -eu
umask 077

: "${ADMIN_PW:?}" "${APP_PW:?}" "${ANALYST_PW:?}"

f="$HOME/.pgpass"
touch "$f"

{
  grep -vE ':(app_admin|app_user|app_analyst):' "$f" || true
  cat <<PGPASS
localhost:5432:*:app_admin:$ADMIN_PW
localhost:5432:*:app_user:$APP_PW
localhost:5432:*:app_analyst:$ANALYST_PW
PGPASS
} > "$f.new"

mv "$f.new" "$f"
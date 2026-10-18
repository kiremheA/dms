#!/usr/bin/env bash
set -eu

touch "$HOME/.pgpass"
chmod 600 "$HOME/.pgpass"

cat >> "$HOME/.pgpass" <<PGPASS
localhost:5432:*:app_admin:$ADMIN_PW
localhost:5432:*:app_user:$APP_PW
localhost:5432:*:app_analyst:$ANALYST_PW
PGPASS
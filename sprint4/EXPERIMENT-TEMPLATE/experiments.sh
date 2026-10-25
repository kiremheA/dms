#!/usr/bin/env bash


set -eu

q() {
  docker compose exec -T "$1" psql -v ON_ERROR_STOP=1 -U course_admin -d service_center -f "/course/sql/$2"
}

x() {
  if q "$@"; then
    echo "ожидалась ошибка, но команда прошла: $*"
    exit 1
  fi
  echo ">>> ошибка ожидаема: $*"
}

wait_sync() {
  for _ in $(seq 60); do
    [ "$(docker compose exec -T primary psql -U course_admin -d service_center -Atc "SELECT count(*) FROM pg_stat_replication WHERE state = 'streaming' AND replay_lsn >= pg_current_wal_lsn()")" = 1 ] && return 0
    sleep 1
  done
  echo "replica не догнала primary за 60 с"
  exit 1
}

wait_sync

echo "=== Эксперимент 2: остановка replica ==="
docker compose stop replica
q primary 01-primary-status.sql
q primary 02-create-probe.sql
x replica 04-read-probes.sql
docker compose up -d --wait
wait_sync
q primary 01-primary-status.sql
q replica 03-replica-status.sql
q replica 04-read-probes.sql

echo "=== Эксперимент 3: остановка primary ==="
docker compose stop primary
x primary 02-create-probe.sql
x replica 02-create-probe.sql
q replica 03-replica-status.sql
q replica 04-read-probes.sql
docker compose up -d --wait
wait_sync
q primary 01-primary-status.sql
q primary 02-create-probe.sql
wait_sync
q replica 04-read-probes.sql

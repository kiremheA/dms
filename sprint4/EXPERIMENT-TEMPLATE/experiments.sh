#!/usr/bin/env bash

q() {
  docker compose exec -T "$1" psql -U course_admin -d service_center -f "/course/sql/$2"
}

echo "=== Эксперимент 2: остановка replica ==="
docker compose stop replica
q primary 01-primary-status.sql
q primary 02-create-probe.sql
q replica 04-read-probes.sql
docker compose up -d --wait
q primary 01-primary-status.sql
q replica 03-replica-status.sql
q replica 04-read-probes.sql

echo "=== Эксперимент 3: остановка primary ==="
docker compose stop primary
q primary 02-create-probe.sql
q replica 02-create-probe.sql
q replica 03-replica-status.sql
q replica 04-read-probes.sql
docker compose up -d --wait
q primary 01-primary-status.sql
q primary 02-create-probe.sql
q replica 04-read-probes.sql
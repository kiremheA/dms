#!/usr/bin/env bash
set -eu

docker compose exec -T primary pg_dump -U course_admin -d service_center -Fc > /tmp/service_center_stand.dump
docker compose exec -T primary pg_restore --list < /tmp/service_center_stand.dump
docker compose --profile restore rm -sf restore
docker volume rm -f sprint04_restore_data
docker compose --profile restore up -d --wait restore
docker compose exec -T restore pg_restore -U course_admin -d course_app_restored --no-acl < /tmp/service_center_stand.dump
docker compose exec -T primary psql -v ON_ERROR_STOP=1 -U course_admin -d service_center -f /course/sql/05-integrity-check.sql
docker compose exec -T restore psql -v ON_ERROR_STOP=1 -U course_admin -d course_app_restored -f /course/sql/05-integrity-check.sql

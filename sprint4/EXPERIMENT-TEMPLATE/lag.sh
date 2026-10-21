#!/usr/bin/env bash

q() {
  docker compose exec -T primary psql -U course_admin -d service_center -c "$1"
}

lag="SELECT pg_wal_lsn_diff(sent_lsn, replay_lsn) AS lag_bytes, write_lag, flush_lag, replay_lag FROM pg_stat_replication"

q "CREATE TABLE app.lag_load AS SELECT g AS id, md5(g::text) AS payload FROM generate_series(1, 2000000) g" &
pid=$!

while kill -0 "$pid" 2>/dev/null; do
  q "$lag"
  sleep 0.5
done

sleep 2
q "$lag"
q "DROP TABLE app.lag_load"
#!/usr/bin/env bash

p() {
  docker compose exec -T "$1" psql -U course_admin -d service_center -Atc "$2"
}

rm -f /tmp/acked.txt
while p primary "INSERT INTO app.replication_probe (marker) VALUES ('loss-' || gen_random_uuid())" > /dev/null; do
  echo >> /tmp/acked.txt
done &

sleep 5

time {
  docker compose kill primary
  p replica "SELECT pg_promote()"
}

wait

acked=$(wc -l < /tmp/acked.txt)
saved=$(p replica "SELECT count(*) FROM app.replication_probe WHERE marker LIKE 'loss-%'")
echo "acked=$acked saved=$saved lost=$((acked - saved))"
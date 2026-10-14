export PGDATABASE=$1
psql -c "SELECT 1 FROM part WHERE id = 1 FOR UPDATE; SELECT pg_sleep(3)" >/dev/null &
sleep 1
time psql -c "SELECT 1 FROM part WHERE id = 1 FOR UPDATE" >/dev/null
wait

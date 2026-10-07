#!/bin/bash
export PGDATABASE=$1
psql -c "SELECT 1 FROM part WHERE id = 1 FOR UPDATE; SELECT pg_sleep(4)" >/dev/null &
sleep 1
psql -c "SELECT 1 FROM part WHERE id = 1 FOR UPDATE" >/dev/null &
sleep 1
psql -x -c "SELECT pid, wait_event_type, wait_event, pg_blocking_pids(pid) AS blocked_by, query
            FROM pg_stat_activity WHERE cardinality(pg_blocking_pids(pid)) > 0" | tee lockwait_result.txt
wait
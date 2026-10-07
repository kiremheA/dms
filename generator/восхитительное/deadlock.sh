#!/bin/bash
export PGDATABASE=$1

run() {
  psql -c "UPDATE part SET price = price WHERE id = 1; SELECT pg_sleep(2); UPDATE part SET price = price WHERE id = 2" >/dev/null &
  psql -c "UPDATE part SET price = price WHERE id = $1; SELECT pg_sleep(2); UPDATE part SET price = price WHERE id = $2" >/dev/null &
  wait
}

{
  echo "разный порядок (deadlock)"; run 2 1
  echo "одинаковый порядок";        run 1 2
} 2>&1 | tee deadlock_result.txt
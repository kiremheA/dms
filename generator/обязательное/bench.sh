#!/bin/bash
export PGDATABASE=$1 PGTZ=UTC

bench() {
  for n in 1 4; do
    for i in 1 2 3 4 5; do
      psql -At -f q$n.sql | tee q${n}_$1.txt | awk '/Execution Time/ {print $3}'
    done | sort -n | sed -n 3p | xargs echo "запрос $n, $1: медиана, мс:"
  done
}

psql -q -c "DROP INDEX IF EXISTS service_order_active_deadline, part_movement_writeoff_created" -c ANALYZE
bench before

psql -q -c "CREATE INDEX service_order_active_deadline ON service_order (deadline_at) WHERE status NOT IN ('issued', 'cancelled');
            CREATE INDEX part_movement_writeoff_created ON part_movement (created_at) WHERE movement_type = 'writeoff'"
bench after
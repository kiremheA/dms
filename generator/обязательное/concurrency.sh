#!/bin/bash
export PGDATABASE=$1
P="psql -qAt"

run_case() {
  PART=$($P -c "INSERT INTO part (name, sku, price) VALUES ('Тест', 'SKU-TEST', 1000) RETURNING id")
  $P -c "INSERT INTO part_movement (part_id, movement_type, quantity, price_at_movement) VALUES ($PART, 'receipt', 1, 1000)"

  for order in 1 2; do
    $P -c "SELECT 1 FROM part WHERE id = $PART $1;
           INSERT INTO part_movement (part_id, service_order_id, movement_type, quantity, price_at_movement)
           SELECT $PART, $order, 'writeoff', 1, 1000
           WHERE NOT EXISTS (SELECT 1 FROM part_movement WHERE part_id = $PART AND movement_type = 'writeoff');
           SELECT pg_sleep(2)" >/dev/null &
  done
  wait

  echo "списаний: $($P -c "SELECT count(*) FROM part_movement WHERE part_id = $PART AND movement_type = 'writeoff'")"
  $P -c "DELETE FROM part_movement WHERE part_id = $PART; DELETE FROM part WHERE id = $PART"
}

echo "без блокировки";             run_case ""
echo "с блокировкой (FOR UPDATE)"; run_case "FOR UPDATE"
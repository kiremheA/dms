SELECT id, status FROM service_order WHERE status = 'in_repair' ORDER BY id LIMIT 2;

BEGIN;
UPDATE master_assignment SET ended_at = now()
WHERE ended_at IS NULL
  AND service_order_id = (SELECT id FROM service_order WHERE status = 'in_repair' ORDER BY id LIMIT 1);
INSERT INTO order_status_history (service_order_id, status)
SELECT id, 'ready_for_pickup' FROM service_order WHERE status = 'in_repair' ORDER BY id LIMIT 1;
UPDATE service_order SET status = 'ready_for_pickup'
WHERE id = (SELECT id FROM service_order WHERE status = 'in_repair' ORDER BY id LIMIT 1)
RETURNING id, status;
COMMIT;

BEGIN;
UPDATE master_assignment SET ended_at = now()
WHERE ended_at IS NULL
  AND service_order_id = (SELECT id FROM service_order WHERE status = 'in_repair' ORDER BY id LIMIT 1);
INSERT INTO order_status_history (service_order_id, status)
SELECT id, 'ready_for_pickup' FROM service_order WHERE status = 'in_repair' ORDER BY id LIMIT 1;
UPDATE service_order SET status = 'ready_for_pickup'
WHERE id = (SELECT id FROM service_order WHERE status = 'in_repair' ORDER BY id LIMIT 1)
RETURNING id, status;
ROLLBACK;

SELECT id, status FROM service_order WHERE status = 'in_repair' ORDER BY id LIMIT 2;
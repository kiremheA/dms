EXPLAIN (ANALYZE, BUFFERS)
SELECT o.id AS order_id, c.full_name AS customer, c.contacts, dc.name AS category,
       o.status, o.deadline_at,
      date_part('day', timestamptz '2026-09-01' - o.deadline_at) AS days_overdue,
       s.full_name AS master
FROM service_order o
JOIN device d ON d.id = o.device_id
JOIN customer c ON c.id = d.customer_id
JOIN device_category dc ON dc.id = d.device_category_id
LEFT JOIN master_assignment a ON a.service_order_id = o.id AND a.ended_at IS NULL
LEFT JOIN staff s ON s.id = a.master_id
WHERE o.status NOT IN ('issued', 'cancelled')
AND o.deadline_at < timestamptz '2026-09-01'
ORDER BY o.deadline_at
LIMIT 10;
EXPLAIN (ANALYZE, BUFFERS)
SELECT p.name AS part, p.sku, sum(m.quantity) AS written_off,
       round(sum(m.quantity * m.price_at_movement), 2) AS amount
FROM part_movement m
JOIN part p ON p.id = m.part_id
WHERE m.movement_type = 'writeoff'
  AND m.created_at >= timestamptz '2026-08-01' AND m.created_at < timestamptz '2026-09-01'
GROUP BY p.id, p.name, p.sku
HAVING sum(m.quantity) >= 500
ORDER BY written_off DESC;
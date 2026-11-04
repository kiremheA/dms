CREATE TEMP TABLE comparison_order AS
SELECT 1 AS id, 2 AS device_id, '{}'::jsonb AS reception_parameters;

UPDATE comparison_order
SET reception_parameters = '{"screen_condition":"трещина в углу"}'::jsonb
WHERE id = 1 AND device_id = 2
RETURNING id, device_id, reception_parameters;

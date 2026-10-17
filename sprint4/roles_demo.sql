\echo === app_admin: разрешено ===
SET ROLE app_admin;
SELECT current_user;
BEGIN;
INSERT INTO device_category (name, default_warranty_days, diagnosis_days) VALUES ('demo', 30, 1);
UPDATE device_category SET diagnosis_days = 2 WHERE name = 'demo';
DELETE FROM device_category WHERE name = 'demo';
CREATE TABLE demo_table (id integer);
ROLLBACK;
\echo === app_admin: запрещено ===
CREATE ROLE demo_role;
RESET ROLE;

\echo === app_user: разрешено ===
SET ROLE app_user;
SELECT current_user;
SELECT count(*) FROM device_category;
BEGIN;
INSERT INTO device_category (name, default_warranty_days, diagnosis_days) VALUES ('demo', 30, 1);
UPDATE device_category SET diagnosis_days = 2 WHERE name = 'demo';
ROLLBACK;

\echo === app_user: запрещено ===
DELETE FROM device_category WHERE name = 'demo';
CREATE TABLE demo_table (id integer);
RESET ROLE;

\echo === app_analyst: разрешено ===
SET ROLE app_analyst;
SELECT current_user;
SELECT count(*) FROM device_category;
\echo === app_analyst: запрещено ===
INSERT INTO device_category (name, default_warranty_days, diagnosis_days) VALUES ('demo', 30, 1);
UPDATE device_category SET diagnosis_days = 2 WHERE name = 'demo';
DELETE FROM device_category WHERE name = 'demo';
RESET ROLE;
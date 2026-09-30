INSERT INTO device_category (name, default_warranty_days, diagnosis_days) VALUES ('Ноутбук', 90, 3);
INSERT INTO customer (full_name, contacts) VALUES ('Петров', 'petrov@example.com');
INSERT INTO staff (full_name, role) VALUES ('Иванов', 'dispatcher');
INSERT INTO device (customer_id, device_category_id, serial_number) VALUES (1, 1, 'NB-SN-0001');
INSERT INTO service_order (device_id, dispatcher_id, warranty_days) VALUES (1, 1, 60);
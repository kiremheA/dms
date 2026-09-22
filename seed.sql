insert into device_category (name, default_warranty_days, diagnosis_days) values
    ('Ноутбук', 90, 1),
    ('Телефон', 60, 1),
    ('Принтер', 30, 1);

insert into repair_norm (device_category_id, complexity, repair_days) values
    (1, 'simple', 3), (1, 'complex', 7),
    (2, 'simple', 2), (2, 'complex', 5),
    (3, 'simple', 3), (3, 'complex', 6);

insert into work_service (device_category_id, name, base_price) values
    (1, 'Замена термопасты', 1500.00),
    (1, 'Замена матрицы', 8000.00),
    (2, 'Замена экрана', 6000.00),
    (3, 'Прочистка картриджа', 800.00);

insert into staff (full_name, role, contacts) values
    ('Шетько Екатерина Станиславовна', 'dispatcher', '+79529325674'),
    ('Бахирева Алена Андреевна', 'master', '+79612507069'),
    ('Ремхе Кирилл Александрович', 'owner', '+79139830655');

insert into master_specialization (staff_id, device_category_id) values
    (2, 1),
    (2, 2);

insert into customer (full_name, contacts) values
    ('Андросова Алена Андреевна', '+79127775643'),
    ('Емеленко Юлия Алексеевна', '+79838723574');

insert into device (customer_id, device_category_id, serial_number, parameters) values
    (1, 1, 'NB-SN-0001', '{"model": "Lenovo ThinkPad T14", "os": "Windows 11"}'),
    (2, 2, 'PH-SN-0001', '{"model": "Samsung Galaxy A54", "memory_gb": 128}');

insert into service_order (device_id, dispatcher_id, warranty_days, reception_parameters) values
    (2, 1, 60, '{"screen_condition": "трещина в углу"}');
insert into order_status_history (service_order_id, status, changed_by) values
    (1, 'queued', 1);
insert into malfunction (service_order_id, description) values
    (1, 'Не включается после падения');

insert into service_order (device_id, dispatcher_id, warranty_days, deadline_at) values
    (1, 1, 90, now() + interval '4 days');
insert into order_status_history (service_order_id, status, changed_by) values
    (2, 'queued', 1), (2, 'diagnosis', 1), (2, 'awaiting_approval', 2), (2, 'in_repair', 1);
insert into malfunction (service_order_id, description) values
    (2, 'Перегрев и произвольное выключение');
update service_order set status = 'in_repair' where id = 2;

insert into master_assignment (service_order_id, master_id) values (2, 2);

insert into diagnosis (service_order_id, master_assignment_id, repair_norm_id, findings) values
    (2, 1, 1, 'Забита система охлаждения, требуется замена термопасты');

insert into estimate (service_order_id) values (2);
insert into estimate_version (estimate_id, version_number, total_amount, status, agreed_at) values
    (1, 1, 1500.00, 'agreed', now());
insert into estimate_line (estimate_version_id, line_type, work_service_id, planned_qty, planned_price) values
    (1, 'service', 1, 1, 1500.00);

insert into part (name, sku, price) values
    ('Матрица 15.6" FHD', 'PRT-MTX-156', 7500.00);
insert into part_movement (part_id, movement_type, quantity, price_at_movement) values
    (1, 'receipt', 5, 7500.00);

insert into device (customer_id, device_category_id, serial_number, parameters) values
    (1, 1, 'NB-SN-0002', '{"model": "Lenovo ThinkPad T14", "os": "Windows 11"}');

insert into service_order (device_id, dispatcher_id, warranty_days, deadline_at) values
    (3, 1, 90, now() - interval '1 day');
insert into order_status_history (service_order_id, status, changed_by) values
    (3, 'queued', 1), (3, 'diagnosis', 1), (3, 'awaiting_approval', 2),
    (3, 'in_repair', 1), (3, 'ready_for_pickup', 2), (3, 'issued', 1);
insert into malfunction (service_order_id, description) values
    (3, 'Разбита матрица');

update service_order set status = 'issued' where id = 3;

insert into master_assignment (service_order_id, master_id, ended_at) values (3, 2, now());

insert into diagnosis (service_order_id, master_assignment_id, repair_norm_id, findings) values
    (3, 2, 2, 'Требуется замена матрицы');

insert into estimate (service_order_id) values (3);
insert into estimate_version (estimate_id, version_number, total_amount, status, agreed_at) values
    (2, 1, 15500.00, 'agreed', now());
insert into estimate_line (estimate_version_id, line_type, work_service_id, planned_qty, planned_price) values
    (2, 'service', 2, 1, 8000.00);
insert into estimate_line (estimate_version_id, line_type, part_id, planned_qty, planned_price) values
    (2, 'part', 1, 1, 7500.00);

insert into part_movement (part_id, service_order_id, movement_type, quantity, price_at_movement) values
    (1, 3, 'writeoff', 1, 7500.00);

insert into work (service_order_id, master_assignment_id, work_service_id, actual_price) values
    (3, 2, 2, 8000.00);

insert into invoice (service_order_id, amount) values
    (3, 15500.00);
insert into payment (invoice_id, amount, received_by) values
    (1, 15500.00, 1);
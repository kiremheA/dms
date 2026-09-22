begin;
insert into service_order (device_id, dispatcher_id, warranty_days)
    values (1, null, 60);
rollback;

begin;
insert into device (customer_id, device_category_id, serial_number)
    values (1, 1, 'NB-SN-0001');
rollback;

begin;
with new_estimate as (
    insert into estimate (service_order_id) values (1) returning id
)
insert into estimate_version (estimate_id, version_number, total_amount, status)
    select id, 1, -500.00, 'draft' from new_estimate;
rollback;

begin;
with new_estimate as (
    insert into estimate (service_order_id) values (1) returning id
), new_version as (
    insert into estimate_version (estimate_id, version_number, total_amount, status)
    select id, 1, 1000.00, 'draft' from new_estimate
    returning id
)
insert into estimate_line (estimate_version_id, line_type, work_service_id, part_id, planned_qty, planned_price)
    select id, 'service', 1, 1, 1, 1000.00 from new_version;
rollback;

begin;
insert into service_order (device_id, dispatcher_id, warranty_days)
    values (9999, 1, 60);
rollback;
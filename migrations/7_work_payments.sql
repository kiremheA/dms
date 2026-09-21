-- Денормализация: service_order_id продублирован из связанного назначения
-- (work.master_assignment_id -> master_assignment.service_order_id) ради
-- прямых выборок "работы по заявке X" без JOIN через назначение (см. отчёт,
-- доп. пояснения, п.13). Согласованность обоих путей — не FK/CHECK, а
-- правило 37/38: проверяется на уровне приложения
CREATE TABLE work (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    service_order_id    integer NOT NULL REFERENCES service_order(id),
    master_assignment_id integer NOT NULL REFERENCES master_assignment(id),
    work_service_id     integer NOT NULL REFERENCES work_service(id),
    actual_price         numeric(10, 2) NOT NULL CHECK (actual_price >= 0),
    created_at          timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE invoice (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    service_order_id    integer NOT NULL UNIQUE REFERENCES service_order(id),
    amount              numeric(10, 2) NOT NULL CHECK (amount >= 0),
    issued_at           timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE payment (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    invoice_id          integer NOT NULL UNIQUE REFERENCES invoice(id),
    amount              numeric(10, 2) NOT NULL CHECK (amount > 0),
    paid_at             timestamptz NOT NULL DEFAULT now(),
    received_by         integer NOT NULL REFERENCES staff(id)
);
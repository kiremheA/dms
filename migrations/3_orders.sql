CREATE TABLE service_order (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id           integer NOT NULL REFERENCES device(id),
    dispatcher_id       integer NOT NULL REFERENCES staff(id),
    order_type          text NOT NULL DEFAULT 'regular'
                             CHECK (order_type IN ('regular', 'warranty')),
    original_order_id   integer REFERENCES service_order(id),
    status              text NOT NULL DEFAULT 'queued' CHECK (status IN (
                             'queued', 'diagnosis', 'awaiting_approval',
                             'in_repair', 'awaiting_part', 'ready_for_pickup',
                             'issued', 'cancelled'
                         )),
    warranty_days       integer NOT NULL CHECK (warranty_days >= 0),
    reception_parameters jsonb NOT NULL DEFAULT '{}'::jsonb, -- та же денормализация, что у device.parameters
    created_at          timestamptz NOT NULL DEFAULT now(),
    deadline_at         timestamptz,
    CHECK (
        (order_type = 'regular' AND original_order_id IS NULL) OR
        (order_type = 'warranty' AND original_order_id IS NOT NULL)
    )
);

CREATE UNIQUE INDEX one_active_order_per_device
    ON service_order (device_id) WHERE status NOT IN ('issued', 'cancelled');

CREATE TABLE order_status_history (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    service_order_id    integer NOT NULL REFERENCES service_order(id),
    status              text NOT NULL,
    changed_at          timestamptz NOT NULL DEFAULT now(),
    changed_by          integer REFERENCES staff(id)
);

CREATE TABLE malfunction (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    service_order_id    integer NOT NULL REFERENCES service_order(id),
    description         text NOT NULL
);
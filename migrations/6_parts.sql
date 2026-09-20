CREATE TABLE part (
    id          integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name        text NOT NULL,
    sku         text NOT NULL UNIQUE,
    price       numeric(10, 2) NOT NULL CHECK (price >= 0)
);

ALTER TABLE estimate_line
    ADD CONSTRAINT estimate_line_part_id_fkey
    FOREIGN KEY (part_id) REFERENCES part(id);

CREATE TABLE part_movement (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    part_id             integer NOT NULL REFERENCES part(id),
    service_order_id    integer REFERENCES service_order(id),
    movement_type       text NOT NULL CHECK (movement_type IN ('receipt', 'writeoff')),
    quantity            numeric(10, 2) NOT NULL CHECK (quantity > 0),
    price_at_movement   numeric(10, 2) NOT NULL CHECK (price_at_movement >= 0),
    created_at          timestamptz NOT NULL DEFAULT now(),
    correction_of_id    integer REFERENCES part_movement(id),
    correction_reason   text,
    CHECK (
        (movement_type = 'writeoff' AND service_order_id IS NOT NULL) OR
        (movement_type = 'receipt' AND service_order_id IS NULL) OR
        (movement_type = 'writeoff' AND correction_of_id IS NOT NULL AND service_order_id IS NULL)
    ),
    CHECK (correction_of_id IS NULL OR correction_reason IS NOT NULL)
);
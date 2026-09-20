CREATE TABLE estimate (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    service_order_id    integer NOT NULL UNIQUE REFERENCES service_order(id)
);

CREATE TABLE estimate_version (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    estimate_id         integer NOT NULL REFERENCES estimate(id),
    version_number      integer NOT NULL CHECK (version_number > 0),
    total_amount        numeric(10, 2) NOT NULL CHECK (total_amount >= 0),
    status              text NOT NULL DEFAULT 'draft'
                             CHECK (status IN ('draft', 'agreed', 'superseded')),
    created_at          timestamptz NOT NULL DEFAULT now(),
    agreed_at           timestamptz,
    UNIQUE (estimate_id, version_number)
);

CREATE UNIQUE INDEX one_agreed_version_per_estimate
    ON estimate_version (estimate_id) WHERE status = 'agreed';

CREATE TABLE estimate_line (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    estimate_version_id integer NOT NULL REFERENCES estimate_version(id),
    line_type           text NOT NULL CHECK (line_type IN ('service', 'part')),
    work_service_id     integer REFERENCES work_service(id),
    part_id             integer,
    planned_qty         numeric(10, 2) NOT NULL CHECK (planned_qty > 0),
    planned_price        numeric(10, 2) NOT NULL CHECK (planned_price >= 0),
    CHECK (
        (line_type = 'service' AND work_service_id IS NOT NULL AND part_id IS NULL) OR
        (line_type = 'part' AND part_id IS NOT NULL AND work_service_id IS NULL)
    )
);
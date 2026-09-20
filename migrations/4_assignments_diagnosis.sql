CREATE TABLE master_assignment (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    service_order_id    integer NOT NULL REFERENCES service_order(id),
    master_id           integer NOT NULL REFERENCES staff(id),
    started_at          timestamptz NOT NULL DEFAULT now(),
    ended_at            timestamptz,
    reason              text,
    CHECK (ended_at IS NULL OR ended_at >= started_at)
);

CREATE UNIQUE INDEX one_current_assignment_per_order
    ON master_assignment (service_order_id) WHERE ended_at IS NULL;

CREATE UNIQUE INDEX one_current_assignment_per_master
    ON master_assignment (master_id) WHERE ended_at IS NULL;

CREATE TABLE diagnosis (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    service_order_id    integer NOT NULL REFERENCES service_order(id),
    master_assignment_id integer NOT NULL UNIQUE REFERENCES master_assignment(id),
    repair_norm_id      integer NOT NULL REFERENCES repair_norm(id),
    findings            text NOT NULL,
    created_at          timestamptz NOT NULL DEFAULT now()
);
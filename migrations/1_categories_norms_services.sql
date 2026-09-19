CREATE TABLE device_category (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name                text NOT NULL UNIQUE,
    default_warranty_days integer NOT NULL CHECK (default_warranty_days >= 0),
    diagnosis_days      integer NOT NULL CHECK (diagnosis_days > 0)
);

CREATE TABLE repair_norm (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_category_id  integer NOT NULL REFERENCES device_category(id),
    complexity          text NOT NULL CHECK (complexity IN ('simple', 'complex')),
    repair_days         integer NOT NULL CHECK (repair_days > 0),
    UNIQUE (device_category_id, complexity)
);

CREATE TABLE work_service (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_category_id  integer NOT NULL REFERENCES device_category(id),
    name                text NOT NULL,
    base_price          numeric(10, 2) NOT NULL CHECK (base_price >= 0)
);
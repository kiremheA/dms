CREATE TABLE staff (
    id          integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    full_name   text NOT NULL,
    role        text NOT NULL CHECK (role IN ('dispatcher', 'master', 'owner')),
    contacts    text
);

CREATE TABLE master_specialization (
    staff_id            integer NOT NULL REFERENCES staff(id),
    device_category_id  integer NOT NULL REFERENCES device_category(id),
    PRIMARY KEY (staff_id, device_category_id)
);

CREATE TABLE customer (
    id          integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    full_name   text NOT NULL,
    contacts    text NOT NULL UNIQUE
);

-- Денормализация: parameters хранит характеристики техники как jsonb,
-- а не отдельной таблицей "атрибут-значение". Набор характеристик
-- зависит от категории и заранее не известен (см. отчёт, "Уточнения",
-- п.1) -- отдельная таблица усложнила бы запросы и потребовала лишних
-- проверок в коде ради небольшого объёма данных
CREATE TABLE device (
    id                  integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    customer_id         integer NOT NULL REFERENCES customer(id),
    device_category_id  integer NOT NULL REFERENCES device_category(id),
    serial_number       text NOT NULL UNIQUE,
    parameters           jsonb NOT NULL DEFAULT '{}'::jsonb
);
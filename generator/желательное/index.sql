\timing on
CREATE TABLE pay_noidx (LIKE payment);
CREATE TABLE pay_idx (LIKE payment);
CREATE INDEX ON pay_idx (paid_at);
INSERT INTO pay_noidx SELECT * FROM payment;
INSERT INTO pay_idx SELECT * FROM payment;
SELECT pg_size_pretty(pg_table_size('pay_noidx')) AS table_without_index,
       pg_size_pretty(pg_table_size('pay_idx'))   AS table_with_index,
       pg_size_pretty(pg_indexes_size('pay_idx')) AS index_size;
DROP TABLE pay_noidx, pay_idx;
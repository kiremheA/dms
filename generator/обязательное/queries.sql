SET timezone = 'UTC';

-- Запрос 1. Просроченные заявки
-- Бизнес-вопрос: какие незавершённые заявки просрочены на заданную дату, кому из клиентов
-- звонить и какой мастер ведёт заявку?
-- Параметры: дата отчёта 2026-09-01; показать 10 самых давних просрочек.
SELECT o.id AS order_id, c.full_name AS customer, c.contacts, dc.name AS category,
       o.status, o.deadline_at,
      date_part('day', timestamptz '2026-09-01' - o.deadline_at) AS days_overdue,
       s.full_name AS master
FROM service_order o
JOIN device d ON d.id = o.device_id
JOIN customer c ON c.id = d.customer_id
JOIN device_category dc ON dc.id = d.device_category_id
LEFT JOIN master_assignment a ON a.service_order_id = o.id AND a.ended_at IS NULL
LEFT JOIN staff s ON s.id = a.master_id
WHERE o.status NOT IN ('issued', 'cancelled')
AND o.deadline_at < timestamptz '2026-09-01'
ORDER BY o.deadline_at
LIMIT 10;

/*
 order_id |            customer             |   contacts   | category |  status   |      deadline_at       | days_overdue |         master          
----------+---------------------------------+--------------+----------+-----------+------------------------+--------------+-------------------------
   585203 | Смирнов Александр Михайлович    | +79000000025 | Смартфон | queued    | 2026-08-18 00:06:45+00 |           13 | 
   585207 | Васильев Олег Михайлович        | +79000011960 | Смартфон | diagnosis | 2026-08-18 00:12:35+00 |           13 | Соколов Артём Игоревич
   585209 | Соловьёв Илья Алексеевич        | +79000002934 | Смартфон | queued    | 2026-08-18 00:15:30+00 |           13 | 
   585218 | Кузьмин Виталий Александрович   | +79000060167 | Смартфон | queued    | 2026-08-18 00:28:38+00 |           13 | 
   585221 | Иванов Евгений Александрович    | +79000057619 | Смартфон | diagnosis | 2026-08-18 00:33:01+00 |           13 | Лебедев Сергей Павлович
   585226 | Орлов Алексей Иванович          | +79000002071 | Смартфон | queued    | 2026-08-18 00:40:19+00 |           13 | 
   585227 | Борисов Александр Игоревич      | +79000064433 | Смартфон | queued    | 2026-08-18 00:41:46+00 |           13 | 
   585228 | Сергеев Александр Александрович | +79000030393 | Смартфон | queued    | 2026-08-18 00:43:14+00 |           13 | 
   585229 | Фёдоров Вячеслав Дмитриевич     | +79000125495 | Смартфон | queued    | 2026-08-18 00:44:41+00 |           13 | 
   585232 | Андреев Павел Алексеевич        | +79000000026 | Смартфон | queued    | 2026-08-18 00:49:04+00 |           13 | 
(10 rows)
*/

-- Запрос 2. Выручка по категориям техники
-- Бизнес-вопрос: сколько денег принесла каждая категория техники за месяц и какой средний чек?
-- Параметры: период оплаты с 2026-08-01 (включительно) по 2026-09-01 (не включая).
SELECT dc.name AS category, count(*) AS paid_orders, sum(p.amount) AS revenue,
     round(avg(p.amount), 2) AS avg_check
FROM payment p
JOIN invoice i ON i.id = p.invoice_id
JOIN service_order o ON o.id = i.service_order_id
JOIN device d ON d.id = o.device_id
JOIN device_category dc ON dc.id = d.device_category_id
WHERE p.paid_at >= timestamptz '2026-08-01' AND p.paid_at < timestamptz '2026-09-01'
GROUP BY dc.name
ORDER BY revenue DESC;

/*     category      | paid_orders |   revenue   | avg_check 
-------------------+-------------+-------------+-----------
 Смартфон          |        8497 | 33252800.00 |   3913.48
 Ноутбук           |        3483 | 19688400.00 |   5652.71
 Телевизор         |        2086 | 14081700.00 |   6750.58
 Планшет           |        2762 | 11355200.00 |   4111.22
 Игровая приставка |        1955 | 10231800.00 |   5233.66
(5 rows)
*/

-- Запрос 3. Обычные и гарантийные заявки
-- Бизнес-вопрос: какую часть обращений составляют гарантийные заявки,
--   за которые сервис денег не получает?
-- Параметры: период создания с 2026-01-01 (включительно) по 2026-09-01 (не включая).
SELECT order_type, count(*) AS orders
FROM service_order
WHERE created_at >= timestamptz '2026-01-01' AND created_at < timestamptz '2026-09-01'
GROUP BY order_type
ORDER BY orders DESC;

/*
 order_type | orders 
------------+--------
 regular    | 221220
 warranty   |  18582
(2 rows)
*/

-- Запрос 4. Запчасти для первоочередной закупки
-- Бизнес-вопрос: какие запчасти израсходованы за месяц в большом количестве и их нужно
--   закупать в первую очередь?
-- Параметры: списания с 2026-08-01 (включительно) по 2026-09-01 (не включая);
--   порог -- не менее 500 штук за период.
SELECT p.name AS part, p.sku, sum(m.quantity) AS written_off,
       round(sum(m.quantity * m.price_at_movement), 2) AS amount
FROM part_movement m
JOIN part p ON p.id = m.part_id
WHERE m.movement_type = 'writeoff'
 AND m.created_at >= timestamptz '2026-08-01' AND m.created_at < timestamptz '2026-09-01'
GROUP BY p.id, p.name, p.sku
HAVING sum(m.quantity) >= 500
ORDER BY written_off DESC;

/*
             part             |   sku    | written_off |   amount    
------------------------------+----------+-------------+-------------
 Дисплейный модуль смартфона  | SKU-0001 |     2245.00 | 10102500.00
 Аккумулятор смартфона        | SKU-0002 |      902.00 |  1353000.00
 Разъём зарядки универсальный | SKU-0003 |      713.00 |   285200.00
 Основная камера смартфона    | SKU-0004 |      613.00 |  1226000.00
 Матрица ноутбука 15.6 дюймов | SKU-0005 |      526.00 |  3156000.00
(5 rows)
*/

-- Запрос 5. Частые неисправности
-- Бизнес-вопрос: с какими неисправностями клиенты обращаются чаще всего
--   (под них нужно держать запас запчастей и обучать мастеров)?
-- Параметры: за всё время; показать неисправности, встретившиеся не менее 100 раз; топ-10.
SELECT description AS malfunction, count(*) AS cases
FROM malfunction
GROUP BY description
HAVING count(*) >= 100
ORDER BY cases DESC
LIMIT 10;

/*
                 malfunction                 | cases  
---------------------------------------------+--------
 Не включается                               | 154840
 Не заряжается или не работает от сети       |  64450
 Нет изображения                             |  49454
 Перегревается и самопроизвольно выключается |  41472
 Попала жидкость                             |  36507
 Механическое повреждение корпуса            |  32906
 Не работает звук                            |  30211
 Самопроизвольно перезагружается             |  28045
 Не работает порт или разъём                 |  26557
 Зависает при работе                         |  25278
(10 rows)
*/
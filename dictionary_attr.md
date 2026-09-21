# Словарь таблиц и атрибутов

База данных сервисного центра по ремонту техники. Всего 20 таблиц.

## device_category

Справочник категорий техники.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| name | text | NOT NULL, UNIQUE | Название категории |
| default_warranty_days | integer | NOT NULL, >= 0 | Гарантийный срок по умолчанию, дней |
| diagnosis_days | integer | NOT NULL, > 0 | Срок диагностики, дней |

## repair_norm

Норма времени на ремонт по категории и сложности.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| device_category_id | integer | NOT NULL, FK → device_category(id) | Категория техники |
| complexity | text | NOT NULL, IN ('simple', 'complex') | Сложность ремонта |
| repair_days | integer | NOT NULL, > 0 | Срок ремонта, дней |
| — | — | UNIQUE (device_category_id, complexity) | Одна норма на пару категория-сложность |

## work_service

Справочник видов работ (услуг).

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| device_category_id | integer | NOT NULL, FK → device_category(id) | Категория техники |
| name | text | NOT NULL | Название услуги |
| base_price | numeric(10,2) | NOT NULL, >= 0 | Базовая цена услуги |

## staff

Сотрудники сервисного центра.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| full_name | text | NOT NULL | ФИО |
| role | text | NOT NULL, IN ('dispatcher', 'master', 'owner') | Роль сотрудника |
| contacts | text | — | Контактные данные |

## master_specialization

Связь мастер—категории техники, на которых он специализируется.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| staff_id | integer | PK (часть), NOT NULL, FK → staff(id) | Мастер |
| device_category_id | integer | PK (часть), NOT NULL, FK → device_category(id) | Категория техники |

## customer

Клиенты сервисного центра.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| full_name | text | NOT NULL | ФИО клиента |
| contacts | text | NOT NULL, UNIQUE | Контактные данные |

## device

Техника клиента.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| customer_id | integer | NOT NULL, FK → customer(id) | Владелец техники |
| device_category_id | integer | NOT NULL, FK → device_category(id) | Категория техники |
| serial_number | text | NOT NULL, UNIQUE | Серийный номер |
| parameters | jsonb | NOT NULL, DEFAULT '{}' | Характеристики техники (набор полей зависит от категории — денормализация) |

## service_order

Заявка на ремонт.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| device_id | integer | NOT NULL, FK → device(id) | Техника |
| dispatcher_id | integer | NOT NULL, FK → staff(id) | Диспетчер, принявший заявку |
| order_type | text | NOT NULL, DEFAULT 'regular', IN ('regular', 'warranty') | Тип заявки |
| original_order_id | integer | FK → service_order(id) | Исходная заявка (для гарантийной) |
| status | text | NOT NULL, DEFAULT 'queued', IN ('queued', 'diagnosis', 'awaiting_approval', 'in_repair', 'awaiting_part', 'ready_for_pickup', 'issued', 'cancelled') | Статус заявки |
| warranty_days | integer | NOT NULL, >= 0 | Гарантийный срок по заявке |
| reception_parameters | jsonb | NOT NULL, DEFAULT '{}' | Параметры техники на момент приёма (денормализация) |
| created_at | timestamptz | NOT NULL, DEFAULT now() | Дата создания заявки |
| deadline_at | timestamptz | — | Плановый срок исполнения |
| — | — | CHECK: regular ⇒ original_order_id IS NULL; warranty ⇒ original_order_id NOT NULL | Согласованность типа и ссылки на исходную заявку |
| — | — | UNIQUE INDEX по device_id WHERE status NOT IN ('issued','cancelled') | У устройства не более одной активной заявки |

## order_status_history

История смены статусов заявки.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| service_order_id | integer | NOT NULL, FK → service_order(id) | Заявка |
| status | text | NOT NULL | Статус, установленный записью |
| changed_at | timestamptz | NOT NULL, DEFAULT now() | Дата и время смены статуса |
| changed_by | integer | FK → staff(id) | Сотрудник, изменивший статус |

## malfunction

Неисправности, заявленные по заявке.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| service_order_id | integer | NOT NULL, FK → service_order(id) | Заявка |
| description | text | NOT NULL | Описание неисправности |

## master_assignment

Назначение мастера на заявку.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| service_order_id | integer | NOT NULL, FK → service_order(id) | Заявка |
| master_id | integer | NOT NULL, FK → staff(id) | Назначенный мастер |
| started_at | timestamptz | NOT NULL, DEFAULT now() | Начало работы над заявкой |
| ended_at | timestamptz | — | Окончание работы над заявкой |
| reason | text | — | Причина завершения/переназначения |
| — | — | CHECK: ended_at IS NULL OR ended_at >= started_at | Корректность интервала |
| — | — | UNIQUE INDEX по service_order_id WHERE ended_at IS NULL | У заявки не более одного текущего назначения |
| — | — | UNIQUE INDEX по master_id WHERE ended_at IS NULL | У мастера не более одного текущего назначения |

## diagnosis

Результат диагностики по назначению мастера.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| service_order_id | integer | NOT NULL, FK → service_order(id) | Заявка |
| master_assignment_id | integer | NOT NULL, UNIQUE, FK → master_assignment(id) | Назначение, в рамках которого сделана диагностика |
| repair_norm_id | integer | NOT NULL, FK → repair_norm(id) | Применённая норма ремонта |
| findings | text | NOT NULL | Результаты диагностики |
| created_at | timestamptz | NOT NULL, DEFAULT now() | Дата диагностики |

## estimate

Смета по заявке.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| service_order_id | integer | NOT NULL, UNIQUE, FK → service_order(id) | Заявка |

## estimate_version

Версия сметы.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| estimate_id | integer | NOT NULL, FK → estimate(id) | Смета |
| version_number | integer | NOT NULL, > 0 | Номер версии |
| total_amount | numeric(10,2) | NOT NULL, >= 0 | Итоговая сумма версии |
| status | text | NOT NULL, DEFAULT 'draft', IN ('draft', 'agreed', 'superseded') | Статус версии |
| created_at | timestamptz | NOT NULL, DEFAULT now() | Дата создания версии |
| agreed_at | timestamptz | — | Дата согласования |
| — | — | UNIQUE (estimate_id, version_number) | Уникальность номера версии в смете |
| — | — | UNIQUE INDEX по estimate_id WHERE status = 'agreed' | У сметы не более одной согласованной версии |

## estimate_line

Позиция версии сметы (услуга или запчасть).

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| estimate_version_id | integer | NOT NULL, FK → estimate_version(id) | Версия сметы |
| line_type | text | NOT NULL, IN ('service', 'part') | Тип позиции |
| work_service_id | integer | FK → work_service(id) | Услуга (если line_type = 'service') |
| part_id | integer | FK → part(id) | Запчасть (если line_type = 'part') |
| planned_qty | numeric(10,2) | NOT NULL, > 0 | Плановое количество |
| planned_price | numeric(10,2) | NOT NULL, >= 0 | Плановая цена за единицу |
| — | — | CHECK: согласованность line_type с заполненностью work_service_id / part_id | Ровно одно из двух полей заполнено по типу позиции |

## part

Справочник запчастей.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| name | text | NOT NULL | Название запчасти |
| sku | text | NOT NULL, UNIQUE | Артикул |
| price | numeric(10,2) | NOT NULL, >= 0 | Текущая цена |

## part_movement

Движение запчастей на складе (поступление/списание).

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| part_id | integer | NOT NULL, FK → part(id) | Запчасть |
| service_order_id | integer | FK → service_order(id) | Заявка (при списании) |
| movement_type | text | NOT NULL, IN ('receipt', 'writeoff') | Тип движения |
| quantity | numeric(10,2) | NOT NULL, > 0 | Количество |
| price_at_movement | numeric(10,2) | NOT NULL, >= 0 | Цена на момент движения |
| created_at | timestamptz | NOT NULL, DEFAULT now() | Дата движения |
| correction_of_id | integer | FK → part_movement(id) | Движение, которое корректируется |
| correction_reason | text | — | Причина корректировки |
| — | — | CHECK: сочетание movement_type / service_order_id / correction_of_id | Списание требует заявку, поступление — нет, кроме сторно |
| — | — | CHECK: correction_of_id IS NULL OR correction_reason IS NOT NULL | Причина обязательна при корректировке |

## work

Фактически выполненная работа по заявке.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| service_order_id | integer | NOT NULL, FK → service_order(id) | Заявка (денормализация от master_assignment_id, правило 37/38) |
| master_assignment_id | integer | NOT NULL, FK → master_assignment(id) | Назначение, в рамках которого выполнена работа |
| work_service_id | integer | NOT NULL, FK → work_service(id) | Вид работы |
| actual_price | numeric(10,2) | NOT NULL, >= 0 | Фактическая цена работы |
| created_at | timestamptz | NOT NULL, DEFAULT now() | Дата выполнения |

## invoice

Счёт на оплату по заявке.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| service_order_id | integer | NOT NULL, UNIQUE, FK → service_order(id) | Заявка |
| amount | numeric(10,2) | NOT NULL, >= 0 | Сумма к оплате |
| issued_at | timestamptz | NOT NULL, DEFAULT now() | Дата выставления счёта |

## payment

Оплата по счёту.

| Атрибут | Тип | Ограничения | Описание |
|---|---|---|---|
| id | integer | PK | Идентификатор |
| invoice_id | integer | NOT NULL, UNIQUE, FK → invoice(id) | Счёт |
| amount | numeric(10,2) | NOT NULL, > 0 | Сумма платежа |
| paid_at | timestamptz | NOT NULL, DEFAULT now() | Дата оплаты |
| received_by | integer | NOT NULL, FK → staff(id) | Сотрудник, принявший оплату |

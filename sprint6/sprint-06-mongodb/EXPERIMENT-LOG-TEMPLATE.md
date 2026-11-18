# Журнал экспериментов MongoDB Replica Set

## Окружение

- Дата и время: 18.11.2026, 11:49–11:58
- Участники: `mongo1:27017`, `mongo2:27017`, `mongo3:27017`
- Commit проекта: `ec80bc0`
- Версия Docker: Docker Compose v2.40.3-desktop.1
- Версия MongoDB: 8.0.30
- Replica Set name: `rs0`
- Драйвер и его версия: MongoDB C++ Driver 4.6.1

## Исходное состояние

| Участник | Роль | Health | Priority | Последняя применённая операция |
|---|---|---:|---:|---|
| `mongo1:27017` | `PRIMARY` | 1 | 2 | 18.11.2026 11:56:55 |
| `mongo2:27017` | `SECONDARY` | 1 | 1 | 18.11.2026 11:56:55 |
| `mongo3:27017` | `SECONDARY` | 1 | 1 | 18.11.2026 11:56:55 |

## Контрольная запись

- Команда: `docker compose --profile tools run --rm --no-deps client /scripts/app-client.sh --file /scripts/create-probe.js`
- `writeConcern`: `{ w: "majority", j: true, wtimeout: 5000 }`
- Результат: подтверждена запись `probe-2026-11-15T15:56:56.781Z-6af9d648666d2bba8520497b` через `mongo1`.
- Чтение с primary: `mongo1:27017`, 2 документа.
- Чтение с secondary: `mongo3:27017`, 2 документа.

## Остановка текущего primary

| Время | Действие или наблюдение | Доказательство |
|---|---|---|
| 18.11.2026 11:56:55 | Зафиксирован текущий primary | `status.js`: `mongo1:27017` — `PRIMARY` |
| 18.11.2026 ≈11:57 | Primary остановлен | `docker compose stop mongo1` |
| 18.11.2026 ≈11:57 | Начались выборы | `mongo1` недоступен; electionId изменился с `...0001` на `...0002` |
| 18.11.2026 11:57:43 | Выбран новый primary | `status.js`: `mongo2:27017` — `PRIMARY` |
| 18.11.2026 11:57:43 | Приложение восстановило запись | `create-probe.js`: запись подтверждена через `mongo2:27017` |

- Продолжительность отсутствия primary: около 12 секунд до проверки нового primary.
- Новый primary: `mongo2:27017`.
- Ошибка приложения во время выборов: retryable write завершилась без ошибки после выбора нового primary.
- Поведение retryable writes: `retryWrites: true`; `mongo1` остановлен во время сценария, запись подтверждена через `mongo2`; marker создан ровно один раз.

## Возврат остановленного участника

- Команда запуска: `docker compose start mongo1`
- Роль после возврата: `PRIMARY` после выборов по priority 2.
- Состояние синхронизации: у трёх участников `health: 1`, `lastApplied` — 18.11.2026 11:58:15.
- Результат проверки контрольной записи: `documentCount: 2`, дубликатов marker нет.

## Потеря большинства

- Какие два участника остановлены: `mongo2` и `mongo3` — secondary относительно `mongo1`.
- Результат записи: majority-запись не подтверждена; у оставшегося `mongo1` `isWritablePrimary: false`, новых контрольных marker не создано.
- Почему primary отсутствует: остался один голос из трёх, при majority = 2.
- Как восстановлено большинство: `docker compose start mongo2 mongo3`; после запуска у всех участников `health: 1`.

## Read/write concerns

- Выбранный `readPreference`: `primary` для критических операций; `secondaryPreferred` для аналитики.
- Выбранный `readConcern`: `majority`; `snapshot` в транзакции.
- Выбранный `writeConcern`: `{ w: "majority", j: true, wtimeout: 5000 }`.
- Влияние на согласованность: подтверждённые записи и чтение единого снимка в транзакции.
- Влияние на доступность и задержку: для critical-операций нужны primary и большинство; аналитика допускает отставание secondary.

## Итоги и ограничения

- Failover выполнен: после остановки `mongo1` запись продолжилась через `mongo2`; после возврата все участники синхронизированы. Потеря majority и retryable write во время выборов проверены.

# Журнал экспериментов спринта 4

## Окружение

- Дата и время: 2026-10-25 15:32
- Участники: Ремхе Кирилл, Бахирева Алена, Шетько Екатерина, Андросова Алена, Липина Юлия
- Версия Docker: Docker 29.1.2 (build 890dcca), Docker Compose v2.40.3-desktop.1
- Версия PostgreSQL: PostgreSQL 17.11 (Debian 17.11-1.pgdg12+2) on aarch64-unknown-linux-gnu, compiled by gcc (Debian 12.2.0-14+deb12u1) 12.2.0, 64-bit
- Commit проекта: 7fb9064
- Особенности конфигурации: потоковая асинхронная репликация (`sync_state=async`), `wal_keep_size=256MB`; роль «приложения» на стенде выполняют клиентские запросы `psql`.

## Эксперимент 1. Репликация primary → replica

| Шаг | Команда или действие | Ожидаемый результат | Фактический результат | Доказательство |
|---:|---|---|---|---|
| 1 | Запуск стенда | Оба узла healthy | `docker compose up -d --wait`: оба узла healthy | `docker compose ps`: оба `(healthy)` |
| 2 | Проверка ролей | Primary: false; replica: true | `01-primary-status.sql` на primary: `is_in_recovery=f`; `03-replica-status.sql` на replica: `t` | `pg_stat_replication`: `streaming`, `async` |
| 3 | Запись на primary | Запись создана | `02-create-probe.sql` на primary: id=1, `probe-20261025-083237-245-301` | `INSERT 0 1`, `primary_wal_lsn=0/3045990` |
| 4 | Чтение на replica | Та же запись найдена | `04-read-probes.sql` на replica: запись id=1 найдена | `last_replayed_lsn=0/30459C8` не меньше `primary_wal_lsn=0/3045990` |
| 5 | Запись на replica | Операция отклонена | `02-create-probe.sql` на replica: `ERROR: cannot execute INSERT in a read-only transaction` | вывод команды на replica |

## Эксперимент 2. Остановка replica

| Шаг | Команда или действие | Наблюдение приложения | Наблюдение БД |
|---:|---|---|---|
| 1 | Остановить replica | `docker compose stop replica`: чтение с replica недоступно, `service "replica" is not running` | `pg_stat_replication`: 0 строк |
| 2 | Выполнить запись на primary | `02-create-probe.sql` на primary: запись проходит, id=2 | primary работает без реплики, `primary_wal_lsn=0/E80D168` |
| 3 | Запустить replica | `docker compose up -d --wait`: чтение с replica снова работает | `streaming`, `async` |
| 4 | Проверить догон реплики | `03-replica-status.sql`, `04-read-probes.sql` на replica: запись id=2 видна рядом с id=1 | `last_replayed_lsn` = `current_wal_lsn` primary = 0/E80D168 |

## Эксперимент 3. Остановка primary

| Шаг | Команда или действие | Наблюдение приложения | Наблюдение БД |
|---:|---|---|---|
| 1 | Остановить primary | `docker compose stop primary`: запись на primary невозможна, `service "primary" is not running` | replica остаётся в recovery (`t`), `pg_stat_wal_receiver` пуст, replay остановлен на 0/E80D1E0 |
| 2 | Проверить запись | `02-create-probe.sql` на primary и replica: запись нигде невозможна, на replica `cannot execute INSERT in a read-only transaction` | автоматического failover нет |
| 3 | Проверить чтение replica | `04-read-probes.sql` на replica: чтение работает, видны id=2 и id=1 | `time_since_last_replayed_transaction` = 7,2 с на момент замера |
| 4 | Восстановить primary | `docker compose up -d --wait`: primary снова принимает запись | оба узла healthy |
| 5 | Проверить репликацию | `01-`, `02-` на primary, `04-` на replica: запись id=3 проходит и видна на replica (ids 3, 2, 1) | `streaming`, `async`, все LSN равны 0/E80D1E0 |

## Эксперимент 4. Backup и restore

| Шаг | Команда или действие | Ожидаемый результат | Фактический результат |
|---:|---|---|---|
| 1 | Создать backup | Архив создан | `pg_dump -Fc`: `/tmp/service_center_stand.dump`, 4,7 КБ |
| 2 | Проверить архив | Список объектов читается | `pg_restore --list`: 17 записей, видны схема `app`, таблица `replication_probe`, PK, UNIQUE, ACL |
| 3 | Запустить чистый restore-узел | Узел healthy | `up -d --wait restore`: чистый том создан заново, узел healthy |
| 4 | Восстановить архив | Ошибок нет | `pg_restore --no-acl`: без ошибок |
| 5 | Проверить данные и ограничения | Проверки пройдены | `05-integrity-check.sql` на primary и restore: результаты совпали (3 пробы, 5 ограничений: NOT NULL, UNIQUE, PRIMARY KEY) |

## Итоги и обнаруженные ограничения

- Репликация асинхронная (`sync_state=async`): запись возможна только на primary, replica доступна для чтения, запись на ней отклоняется. Остановка replica не мешает работе primary, после запуска replica догоняет его без вмешательства.
- Автоматического переключения нет: при остановке primary запись недоступна на обоих узлах, чтение с replica работает до возврата primary.
- Backup восстанавливается в чистый узел без ошибок. Права (ACL) из дампа не восстанавливаются (`--no-acl`), роли в дамп базы не входят.
- Проверка целостности на стенде охватывает единственную таблицу стенда `app.replication_probe`, данные сервиса сверялись локально на `service_center_load`.
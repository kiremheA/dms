# Инфраструктурный шаблон спринта 6

Учебный стенд MongoDB Replica Set для обязательной части спринта «Индексы, транзакции и репликация». Он запускает три полноценных участника, автоматически инициирует Replica Set, создаёт пользователей и позволяет воспроизвести выбор нового primary после отказа текущего.

Шаблон не реализует за студента bulk-сценарий, бизнес-транзакцию и индексы проекта: эти решения должны быть связаны с предметной областью. Он предоставляет инфраструктуру, в которой их можно корректно выполнять.

## Архитектура

```text
                   Replica Set rs0

             ┌──────────┐
             │  mongo1  │ priority 2
             └────┬─────┘
                  │ replication / election
          ┌───────┴────────┐
          │                │
    ┌─────▼────┐     ┌─────▼────┐
    │  mongo2  │     │  mongo3  │
    │priority 1│     │priority 1│
    └──────────┘     └──────────┘
```

Все три участника хранят данные и голосуют. Большинство равно двум, поэтому после остановки одного узла оставшиеся два могут выбрать primary. Arbiter намеренно не используется.

Должны быть свободны порты `27017`–`27019` либо другие значения в `.env`.

Порты публикуются только на `127.0.0.1`. Полноценные клиенты Replica Set должны работать в Docker-сети `sprint06_mongodb_network`

Запуск Replica Set

```powershell
docker compose up -d keyfile-init mongo1 mongo2 mongo3 rs-init
docker compose ps --all
```

Посмотреть журналы без раскрытия паролей:

```powershell
docker compose logs --tail 100 mongo1 mongo2 mongo3 rs-init
```

Проверка топологии:

```powershell
docker compose --profile tools run --rm --no-deps client /scripts/root-client.sh --file /scripts/status.js
```

Должны присутствовать один `PRIMARY` и два `SECONDARY`. Объяснить выбор для `PRIMARY` и `SECONDARY`

Контрольная запись и чтение

Запись выполняется с `{ w: "majority", j: true, wtimeout: 5000 }`:

```powershell
docker compose --profile tools run --rm --no-deps client /scripts/app-client.sh --file /scripts/create-probe.js
```

Чтение использует `secondaryPreferred` и `readConcern: "majority"`:

```powershell
docker compose --profile tools run --rm --no-deps client /scripts/app-client.sh --file /scripts/read-probes.js
docker compose --profile tools run --rm --no-deps client /scripts/app-client.sh --file /scripts/integrity-check.js
```

Разобраться что как работает oplog

Выбор нового primary

1. Зафиксируйте текущий primary
2. Остановите именно соответствующий сервис.

```powershell
docker compose stop mongo1
```

3. Подождите 10–30 секунд и проверьте состояние через оставшиеся seed hosts:

```powershell
docker compose --profile tools run --rm --no-deps client /scripts/root-client.sh --file /scripts/status.js
```

4. Убедитесь, что один из оставшихся участников стал `PRIMARY`.
5. Выполните новую запись — драйвер должен направить её новому primary:

```powershell
docker compose --profile tools run --rm --no-deps client /scripts/app-client.sh --file /scripts/create-probe.js
```

6. Верните остановленного участника и проверьте его роль:

```powershell
docker compose start mongo1
docker compose --profile tools run --rm --no-deps client /scripts/root-client.sh --file /scripts/status.js
```

После возврата `mongo1` сначала присоединяется как secondary. Из-за более высокого priority позднее он может снова инициировать выборы и стать primary — это ожидаемое поведение, которое следует зафиксировать.

Если исходный primary оказался не `mongo1`, замените имя сервиса во всех командах. Не останавливайте узел по предположению.

Проверка потери большинства

После восстановления трёх здоровых участников остановите два secondary относительно текущего primary. При одном оставшемся голосе primary не сможет сохранять роль и majority-записи станут недоступны.

Зафиксируйте ошибку и восстановите оба узла. Этот опыт показывает различие между доступностью отдельного процесса `mongod` и наличием работоспособного большинства Replica Set.

Read concern, write concern и read preference

Для каждой операции проекта зафиксируйте собственное решение в [CONCERNS-DECISION-TEMPLATE.md](CONCERNS-DECISION-TEMPLATE.md).

Базовые ориентиры:

- транзакции выполняются с `readPreference: primary`;
- `writeConcern: { w: "majority", j: true, wtimeout: 5000 }` уменьшает риск отката подтверждённой записи, но увеличивает задержку и требует доступного большинства;
- `readConcern: "majority"` не возвращает данные, которые не подтверждены большинством;
- `secondaryPreferred` может разгрузить primary для допустимой к устареванию аналитики, но не подходит для любого чтения после записи;
- таймаут write concern не доказывает, что запись отсутствует: она могла примениться на части участников, поэтому повтор операции должен быть идемпотентным.

Подключение приложения

Добавьте контейнер приложения в существующую сеть:

```yaml
services:
  application:
    networks:
      - sprint06_mongodb

networks:
  sprint06_mongodb:
    external: true
    name: sprint06_mongodb_network
```

Внутри этой сети используйте seed list:

```text
mongodb://<app-user>:<url-encoded-password>@mongo1:27017,mongo2:27017,mongo3:27017/<database>?replicaSet=rs0&authSource=<database>&retryWrites=true&w=majority
```

Пароль в URI должен быть URL-encoded. Не фиксируйте URI с настоящим паролем в Git.

Подключение к опубликованному `localhost:27017` с `directConnection=true` допустимо только для диагностики отдельного узла. Оно не проверяет обнаружение топологии и автоматическое переключение драйвера.

Транзакции и bulk API

Replica Set позволяет выполнять многошаговые транзакции. В проекте необходимо реализовать через официальный драйвер:

- бизнес-операцию минимум по двум коллекциям;
- успешный commit;
- откат после ошибки;
- обработку transient transaction error;
- `readPreference: primary` и обоснованные transaction-level concerns.

Bulk-сценарий также выполняется через официальный драйвер и должен содержать минимум два типа операций, выбранный ordered/unordered режим и обработку частичной ошибки. Инфраструктурный шаблон намеренно не подменяет эти проектные решения искусственными shell-примерами.

Остановка и чистый сброс

Остановить контейнеры с сохранением данных:

```powershell
docker compose --profile tools down
```

Полный сброс удаляет данные всех узлов и keyfile volume:

```powershell
docker compose --profile tools down --volumes
```

Полный сброс необратим. Выполняйте его только после проверки точного каталога шаблона и сохранения нужных доказательств. После сброса можно сохранить прежний файл `secrets/mongo-keyfile` или явно создать новый.

Диагностика

```powershell
docker compose ps --all
docker compose logs --tail 100 keyfile-init rs-init
docker compose logs --tail 100 mongo1 mongo2 mongo3
```

Частые причины ошибок:

- не создан `secrets/mongo-keyfile`;
- keyfile был изменён после инициализации volumes;
- в `.env` остались `change_me_...`;
- заняты локальные порты;
- приложение подключено только к одному узлу;
- приложение находится вне `sprint06_mongodb_network` и не разрешает имена участников;
- остановлены два из трёх голосующих участников, поэтому нет большинства.

Официальные справочные материалы:

- [Replica Set и keyfile authentication](https://www.mongodb.com/docs/manual/tutorial/deploy-replica-set-with-keyfile-access-control/)
- [Replication fundamentals](https://www.mongodb.com/docs/manual/replication/)
- [Write concern](https://www.mongodb.com/docs/manual/reference/write-concern/)
- [Transactions](https://www.mongodb.com/docs/manual/core/transactions/)
- [Docker Official Image](https://hub.docker.com/_/mongo/)

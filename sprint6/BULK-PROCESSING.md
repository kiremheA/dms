# Bulk-операции

В `mongo_bulk_demo.cpp` один `bulk_write` содержит:

1. `update_one` для заявки `10001`;
2. дублирующий `insert_one` для `10001` — ошибка `11000`;
3. `insert_one` для новой заявки `10002`.

| Режим | После ошибки |
|---|---|
| `ordered(true)` | `update_one = 1`, `insert_one = 0` |
| `unordered(false)` | `update_one = 1`, `insert_one = 1` |

Ошибка дубликата обрабатывается в обоих случаях. Для независимых заявок выбран `unordered`: одна ошибка не останавливает остальные операции.

```sh
cmake --build /private/tmp/dms-sprint6-build --target mongo_bulk_demo
DMS_MONGO_URI='mongodb://localhost:27017' /private/tmp/dms-sprint6-build/mongo_bulk_demo
```

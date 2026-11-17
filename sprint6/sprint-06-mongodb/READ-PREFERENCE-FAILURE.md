# Read preference

Остановлен `mongo3`. Чтение из `replication_probe` выполнялось с `readConcern: "majority"`.

| Режим | Узел | Документов |
|---|---|---:|
| `primary` | `mongo1:27017` | 2 |
| `secondary` | `mongo2:27017` | 2 |
| `secondaryPreferred` | `mongo2:27017` | 2 |

После запуска `mongo3` все три узла снова имеют `health: 1`.

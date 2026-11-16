# Транзакция MongoDB

`mongo_transaction_demo.cpp` меняет профиль устройства и параметры его приёма в одной транзакции.

- Успех: обе операции подтверждены.
- Ошибка между операциями: профиль пытается сменить `memory_gb` с `512` на `1024`, затем вызывается `abort_transaction()`. В базе остаются `memory_gb: 512` и `screen_condition: "cracked"`.
- Временная ошибка: при `TransientTransactionError` транзакция отменяется и запускается заново.

Настройки: `readConcern: "snapshot"`, `writeConcern: { w: "majority", j: true, wtimeout: 5000 }`.

```sh
cmake --build /private/tmp/dms-sprint6-build --target mongo_transaction_demo
DMS_MONGO_URI='...' /private/tmp/dms-sprint6-build/mongo_transaction_demo
```

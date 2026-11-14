# dms

Сборка:

```sh
cmake -S . -B build
cmake --build build
./build/dms_app
```

Настройки подключения передаются через окружение:

```sh
export DMS_PG_CONNINFO='host=localhost port=5432 dbname=service_center user=app_user'
export DMS_PG_ANALYST_CONNINFO='host=localhost port=5432 dbname=service_center user=app_analyst'
export DMS_MONGO_URI='mongodb://localhost:27017'
export DMS_MONGO_DB='service_center'
```

После первого запуска можно загрузить минимальные определения полей:

```sh
mongosh "$DMS_MONGO_URI/$DMS_MONGO_DB" sprint5/seed_mongo.js
```
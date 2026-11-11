#include "main_window.hpp"
#include "mongo_schema.hpp"

#include <QApplication>
#include <QMessageBox>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace {

QString envOr(const char* key, const QString& fallback) {
    const char* value = std::getenv(key);
    return value && *value ? QString::fromUtf8(value) : fallback;
}

}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Сервисный центр");
    QApplication::setOrganizationName("Учебный проект БД");

    try {
        const QString pgConninfo = envOr(
            "DMS_PG_CONNINFO",
            "host=localhost port=5432 dbname=service_center user=app_user");
        const QString mongoUri = envOr("DMS_MONGO_URI", "mongodb://localhost:27017");
        const QString mongoName = envOr("DMS_MONGO_DB", "service_center");

        PgDatabase pg(pgConninfo);
        mongocxx::instance mongoInstance{};
        mongocxx::client mongoClient{mongocxx::uri{mongoUri.toStdString()}};
        auto mongoDb = mongoClient[mongoName.toStdString()];
        mongo_schema::initialize(mongoDb);

        Actor actor;
        if (!MainWindow::chooseActor(pg, actor)) return 0;

        MainWindow window(pg, mongoDb, actor);
        window.show();
        return app.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, "Не удалось запустить приложение",
                              QString::fromUtf8(error.what()));
        return 1;
    }
}

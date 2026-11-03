#include "mongo_crud.hpp"
#include "mongo_schema.hpp"

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

int main() {
    try {
        const int orderId = 1, deviceId = 2, categoryId = 2;
        mongocxx::instance instance{};
        mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};
        const std::string name = "sprint5_verify";
        client[name].drop();
        auto db = client[name];

        mongo_schema::initialize(db);
        db["parameter_definitions"].insert_one(make_document(
            kvp("name", "screen_condition"), kvp("scope", "intake"),
            kvp("type", "string"), kvp("required", true),
            kvp("category_ids", make_array(categoryId))));

        MongoCrud crud{db};
        auto empty = make_document();
        crud.createDeviceParameters(deviceId, categoryId, empty.view());
        auto profile = crud.readDeviceParameters(deviceId);
        if (!profile) throw std::runtime_error("Профиль не найден");
        auto parameters = make_document(kvp("screen_condition", "трещина в углу"));
        crud.createOrderIntakeParameters(
            orderId, profile->view()["_id"].get_oid().value, parameters.view());
        auto saved = crud.readOrderIntakeParameters(orderId);
        if (!saved) throw std::runtime_error("Параметры приёма не найдены");

        std::cout << "id | device_id | reception_parameters\n"
                  << orderId << " | " << deviceId << " | "
                  << bsoncxx::to_json(
                         saved->view()["reception_parameters"].get_document().view())
                  << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include "mongo_schema.hpp"

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/oid.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/model/insert_one.hpp>
#include <mongocxx/model/update_one.hpp>
#include <mongocxx/options/bulk_write.hpp>
#include <mongocxx/uri.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

namespace {

struct BulkOutcome {
    bool duplicateKeyHandled;
    std::int64_t updated;
    std::int64_t inserted;
    std::int64_t documents;
};

std::string mongoUri() {
    const char* value = std::getenv("DMS_MONGO_URI");
    return value && *value ? value : "mongodb://localhost:27017";
}

BulkOutcome runScenario(mongocxx::client& client, const char* databaseName,
                        bool ordered) {
    auto database = client[databaseName];
    database.drop();
    mongo_schema::initialize(database);

    const bsoncxx::oid existingProfileId;
    const bsoncxx::oid newProfileId;
    auto profiles = database["device_parameters"];
    profiles.insert_one(make_document(
        kvp("_id", existingProfileId), kvp("device_id", 10001),
        kvp("category_id", 1),
        kvp("parameters", make_document(kvp("model", "Notebook 10001"),
                                          kvp("memory_gb", 256)))));
    profiles.insert_one(make_document(
        kvp("_id", newProfileId), kvp("device_id", 10002),
        kvp("category_id", 1),
        kvp("parameters", make_document(kvp("model", "Notebook 10002"),
                                          kvp("memory_gb", 512)))));

    auto intakes = database["order_intake_parameters"];
    intakes.insert_one(make_document(
        kvp("service_order_id", 10001), kvp("device_profile_id", existingProfileId),
        kvp("reception_parameters", make_document(kvp("screen_condition", "intact")))));

    mongocxx::model::update_one updateExisting{
        make_document(kvp("service_order_id", 10001)),
        make_document(kvp("$set", make_document(
            kvp("reception_parameters", make_document(
                kvp("screen_condition", "cracked"))))))};
    mongocxx::model::insert_one duplicateOrder{make_document(
        kvp("service_order_id", 10001), kvp("device_profile_id", existingProfileId),
        kvp("reception_parameters", make_document(
            kvp("screen_condition", "cracked"))))};
    mongocxx::model::insert_one insertNew{make_document(
        kvp("service_order_id", 10002), kvp("device_profile_id", newProfileId),
        kvp("reception_parameters", make_document(
            kvp("screen_condition", "cracked"))))};

    mongocxx::options::bulk_write options;
    options.ordered(ordered);
    auto bulk = intakes.create_bulk_write(options);
    bulk.append(updateExisting);
    bulk.append(duplicateOrder);
    bulk.append(insertNew);

    bool duplicateKeyHandled = false;
    try {
        bulk.execute();
    } catch (const mongocxx::bulk_write_exception& error) {
        if (error.code().value() != 11000) {
            throw;
        }
        duplicateKeyHandled = true;
    }

    return {
        duplicateKeyHandled,
        intakes.count_documents(make_document(
            kvp("service_order_id", 10001),
            kvp("reception_parameters.screen_condition", "cracked"))),
        intakes.count_documents(make_document(
            kvp("service_order_id", 10002),
            kvp("reception_parameters.screen_condition", "cracked"))),
        intakes.count_documents({})
    };
}

}

int main() {
    try {
        mongocxx::instance instance{};
        mongocxx::client client{mongocxx::uri{mongoUri()}};

        const auto ordered = runScenario(
            client, "sprint6_bulk_ordered_demo", true);
        const auto unordered = runScenario(
            client, "sprint6_bulk_unordered_demo", false);

        if (!ordered.duplicateKeyHandled || ordered.updated != 1 ||
            ordered.inserted != 0 || ordered.documents != 1 ||
            !unordered.duplicateKeyHandled || unordered.updated != 1 ||
            unordered.inserted != 1 || unordered.documents != 2) {
            throw std::runtime_error("Сравнение ordered и unordered не пройдено");
        }

        std::cout << "ordered: duplicate_key_handled, update_one=1, insert_one=0\n"
                  << "unordered: duplicate_key_handled, update_one=1, insert_one=1\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include "mongo_schema.hpp"

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>

#include <algorithm>
#include <string>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

namespace {

void ensureCollection(mongocxx::database& db, const std::string& name,
                      const char* validatorJson) {
    auto validator = bsoncxx::from_json(validatorJson);
    auto names = db.list_collection_names();
    const bool exists = std::find(names.begin(), names.end(), name) != names.end();

    const std::string operation = exists ? "collMod" : "create";
    auto command = make_document(
        kvp(operation, name),
        kvp("validator", validator.view()),
        kvp("validationLevel", "strict"),
        kvp("validationAction", "error"));
    db.run_command(command.view());
}

void ensureIndex(mongocxx::database& db, const std::string& collection,
                 const std::string& name, bsoncxx::document::view keys,
                 bool unique) {
    auto index = make_document(
        kvp("key", keys), kvp("name", name), kvp("unique", unique));
    auto command = make_document(
        kvp("createIndexes", collection),
        kvp("indexes", make_array(index.view())));
    db.run_command(command.view());
}

}

void mongo_schema::initialize(mongocxx::database& db) {
    ensureCollection(db, "device_parameters", R"({
        "$jsonSchema": {
            "bsonType": "object",
            "required": ["device_id", "category_id", "parameters"],
            "properties": {
                "device_id": {"bsonType": "int", "minimum": 1},
                "category_id": {"bsonType": "int", "minimum": 1},
                "parameters": {"bsonType": "object"}
            }
        }
    })");
    ensureCollection(db, "order_intake_parameters", R"({
        "$jsonSchema": {
            "bsonType": "object",
            "required": ["service_order_id", "device_profile_id", "reception_parameters"],
            "properties": {
                "service_order_id": {"bsonType": "int", "minimum": 1},
                "device_profile_id": {"bsonType": "objectId"},
                "reception_parameters": {"bsonType": "object"}
            }
        }
    })");
    ensureCollection(db, "parameter_definitions", R"({
        "$jsonSchema": {
            "bsonType": "object",
            "required": ["name", "scope", "type", "required", "category_ids"],
            "properties": {
                "name": {"bsonType": "string"},
                "scope": {"bsonType": "string", "enum": ["device", "intake"]},
                "type": {"bsonType": "string", "enum": ["string", "number", "boolean"]},
                "required": {"bsonType": "bool"},
                "category_ids": {
                    "bsonType": "array",
                    "minItems": 1,
                    "uniqueItems": true,
                    "items": {"bsonType": "int", "minimum": 1}
                }
            }
        }
    })");

    ensureIndex(db, "device_parameters", "device_id_unique",
                make_document(kvp("device_id", 1)).view(), true);
    ensureIndex(db, "order_intake_parameters", "service_order_id_unique",
                make_document(kvp("service_order_id", 1)).view(), true);
    ensureIndex(db, "order_intake_parameters", "device_profile_id",
                make_document(kvp("device_profile_id", 1)).view(), false);
    ensureIndex(db, "parameter_definitions", "scope_name_unique",
                make_document(kvp("scope", 1), kvp("name", 1)).view(), true);
}

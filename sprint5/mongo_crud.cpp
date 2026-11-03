#include "mongo_crud.hpp"

#include <bsoncxx/builder/basic/document.hpp>
#include <mongocxx/collection.hpp>

#include <cmath>
#include <map>
#include <stdexcept>
#include <string>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

namespace {

std::string stringValue(bsoncxx::document::element field) {
    auto value = field.get_string().value;
    return {value.data(), value.size()};
}

}

MongoCrud::MongoCrud(mongocxx::database& db) : db_(db) {}

void MongoCrud::validateParameters(int categoryId, const char* scope,
                                   bsoncxx::document::view values) {
    struct Definition {
        std::string type;
        bool required;
    };
    std::map<std::string, Definition> definitions;
    auto filter = make_document(
        kvp("scope", scope), kvp("category_ids", categoryId));

    for (auto&& definition : db_["parameter_definitions"].find(filter.view())) {
        definitions.emplace(
            stringValue(definition["name"]),
            Definition{stringValue(definition["type"]),
                       definition["required"].get_bool().value});
    }

    for (const auto& [name, definition] : definitions) {
        if (definition.required && !values[name]) {
            throw std::runtime_error("Не заполнено обязательное поле: " + name);
        }
    }

    for (auto field : values) {
        const auto key = field.key();
        const std::string name(key.data(), key.size());
        auto definition = definitions.find(name);
        if (definition == definitions.end()) {
            throw std::runtime_error("Поле не определено для категории и типа данных: " + name);
        }

        const auto type = field.type();
        const auto& expected = definition->second.type;
        const bool number = type == bsoncxx::type::k_int32 ||
            type == bsoncxx::type::k_int64 ||
            (type == bsoncxx::type::k_double && std::isfinite(field.get_double().value));
        const bool matches = (expected == "string" && type == bsoncxx::type::k_string) ||
            (expected == "boolean" && type == bsoncxx::type::k_bool) ||
            (expected == "number" && number);
        if (!matches) {
            throw std::runtime_error("Неверный тип поля: " + name + "; ожидался " + expected);
        }
    }
}

void MongoCrud::createDeviceParameters(int deviceId, int categoryId,
                                       bsoncxx::document::view parameters) {
    validateParameters(categoryId, "device", parameters);
    db_["device_parameters"].insert_one(make_document(
        kvp("device_id", deviceId), kvp("category_id", categoryId),
        kvp("parameters", parameters)));
}

bsoncxx::stdx::optional<bsoncxx::document::value>
MongoCrud::readDeviceParameters(int deviceId) {
    return db_["device_parameters"].find_one(make_document(kvp("device_id", deviceId)));
}

bool MongoCrud::updateDeviceParameters(int deviceId,
                                       bsoncxx::document::view parameters) {
    auto profile = readDeviceParameters(deviceId);
    if (!profile) throw std::runtime_error("Профиль устройства не найден");
    const int categoryId = profile->view()["category_id"].get_int32().value;
    validateParameters(categoryId, "device", parameters);

    auto update = make_document(kvp("$set", make_document(kvp("parameters", parameters))));
    auto result = db_["device_parameters"].update_one(
        make_document(kvp("device_id", deviceId)), update.view());
    return result && result->matched_count() > 0;
}

bool MongoCrud::deleteDeviceParameters(int deviceId) {
    auto profile = readDeviceParameters(deviceId);
    if (!profile) return false;

    const auto profileId = profile->view()["_id"].get_oid().value;
    auto intake = db_["order_intake_parameters"].find_one(
        make_document(kvp("device_profile_id", profileId)));
    if (intake) {
        throw std::runtime_error("Нельзя удалить профиль устройства, на который ссылаются приёмы");
    }

    auto result = db_["device_parameters"].delete_one(
        make_document(kvp("device_id", deviceId)));
    return result && result->deleted_count() > 0;
}

void MongoCrud::createOrderIntakeParameters(int serviceOrderId,
                                            const bsoncxx::oid& deviceProfileId,
                                            bsoncxx::document::view parameters) {
    auto profile = db_["device_parameters"].find_one(
        make_document(kvp("_id", deviceProfileId)));
    if (!profile) throw std::runtime_error("Профиль устройства для приёма не найден");

    const int categoryId = profile->view()["category_id"].get_int32().value;
    validateParameters(categoryId, "intake", parameters);
    db_["order_intake_parameters"].insert_one(make_document(
        kvp("service_order_id", serviceOrderId),
        kvp("device_profile_id", deviceProfileId),
        kvp("reception_parameters", parameters)));
}

bsoncxx::stdx::optional<bsoncxx::document::value>
MongoCrud::readOrderIntakeParameters(int serviceOrderId) {
    return db_["order_intake_parameters"].find_one(
        make_document(kvp("service_order_id", serviceOrderId)));
}

bool MongoCrud::updateOrderIntakeParameters(
    int serviceOrderId, bsoncxx::document::view parameters) {
    auto intake = readOrderIntakeParameters(serviceOrderId);
    if (!intake) throw std::runtime_error("Параметры приёма не найдены");

    const auto profileId = intake->view()["device_profile_id"].get_oid().value;
    auto profile = db_["device_parameters"].find_one(make_document(kvp("_id", profileId)));
    if (!profile) throw std::runtime_error("Профиль устройства для приёма не найден");

    const int categoryId = profile->view()["category_id"].get_int32().value;
    validateParameters(categoryId, "intake", parameters);
    auto update = make_document(
        kvp("$set", make_document(kvp("reception_parameters", parameters))));
    auto result = db_["order_intake_parameters"].update_one(
        make_document(kvp("service_order_id", serviceOrderId)), update.view());
    return result && result->matched_count() > 0;
}

bool MongoCrud::deleteOrderIntakeParameters(int serviceOrderId) {
    auto result = db_["order_intake_parameters"].delete_one(
        make_document(kvp("service_order_id", serviceOrderId)));
    return result && result->deleted_count() > 0;
}

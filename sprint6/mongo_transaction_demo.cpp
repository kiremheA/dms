#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/oid.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/client_session.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/transaction.hpp>
#include <mongocxx/read_concern.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/write_concern.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

namespace {

std::string mongoUri() {
    const char* value = std::getenv("DMS_MONGO_URI");
    if (!value || !*value) {
        throw std::runtime_error("Укажите DMS_MONGO_URI для Replica Set");
    }
    return value;
}

mongocxx::options::transaction transactionOptions() {
    mongocxx::read_concern readConcern;
    readConcern.acknowledge_string("snapshot");
    mongocxx::write_concern writeConcern;
    writeConcern.majority(std::chrono::milliseconds{5000});
    writeConcern.journal(true);

    mongocxx::options::transaction options;
    options.read_concern(readConcern);
    options.write_concern(writeConcern);
    return options;
}

void updateDiagnosis(mongocxx::client_session& session,
                     mongocxx::collection& profiles,
                     mongocxx::collection& intakes,
                     int memoryGb, const char* screenCondition) {
    profiles.update_one(
        session,
        make_document(kvp("device_id", 20001)),
        make_document(kvp("$set", make_document(
            kvp("parameters.memory_gb", memoryGb)))));
    intakes.update_one(
        session,
        make_document(kvp("service_order_id", 20001)),
        make_document(kvp("$set", make_document(
            kvp("reception_parameters.screen_condition", screenCondition)))));
}

}

int main() {
    try {
        mongocxx::instance instance{};
        mongocxx::client client{mongocxx::uri{mongoUri()}};
        auto database = client["course_app"];
        auto profiles = database["transaction_device_parameters"];
        auto intakes = database["transaction_order_intake_parameters"];
        profiles.drop();
        intakes.drop();
        profiles.create_index(make_document(kvp("device_id", 1)),
                              mongocxx::options::index{}.unique(true));
        intakes.create_index(make_document(kvp("service_order_id", 1)),
                             mongocxx::options::index{}.unique(true));

        const bsoncxx::oid profileId;
        profiles.insert_one(make_document(
            kvp("_id", profileId), kvp("device_id", 20001), kvp("category_id", 1),
            kvp("parameters", make_document(kvp("model", "Notebook 20001"),
                                              kvp("memory_gb", 256)))));
        intakes.insert_one(make_document(
            kvp("service_order_id", 20001), kvp("device_profile_id", profileId),
            kvp("reception_parameters", make_document(kvp("screen_condition", "intact")))));

        const auto options = transactionOptions();

        auto committedSession = client.start_session();
        committedSession.start_transaction(options);
        updateDiagnosis(committedSession, profiles, intakes, 512, "cracked");
        committedSession.commit_transaction();

        if (profiles.count_documents(make_document(
                kvp("device_id", 20001), kvp("parameters.memory_gb", 512))) != 1 ||
            intakes.count_documents(make_document(
                kvp("service_order_id", 20001),
                kvp("reception_parameters.screen_condition", "cracked"))) != 1) {
            throw std::runtime_error("Проверка успешной транзакции не пройдена");
        }

        auto rollbackSession = client.start_session();
        bool errorInjected = false;
        bool rolledBack = false;
        rollbackSession.start_transaction(options);
        try {
            profiles.update_one(
                rollbackSession,
                make_document(kvp("device_id", 20001)),
                make_document(kvp("$set", make_document(
                    kvp("parameters.memory_gb", 1024)))));
            errorInjected = true;
            throw std::runtime_error("Внедрённая ошибка между операциями транзакции");
        } catch (...) {
            rollbackSession.abort_transaction();
            if (!errorInjected) {
                throw;
            }
            rolledBack = true;
        }

        if (!rolledBack ||
            profiles.count_documents(make_document(
                kvp("device_id", 20001), kvp("parameters.memory_gb", 512))) != 1 ||
            intakes.count_documents(make_document(
                kvp("service_order_id", 20001),
                kvp("reception_parameters.screen_condition", "cracked"))) != 1) {
            throw std::runtime_error("Проверка отката транзакции не пройдена");
        }

        auto blockingSession = client.start_session();
        blockingSession.start_transaction(options);
        profiles.update_one(
            blockingSession,
            make_document(kvp("device_id", 20001)),
            make_document(kvp("$set", make_document(
                kvp("parameters.memory_gb", 768)))));

        auto retrySession = client.start_session();
        bool transientErrorHandled = false;
        try {
            retrySession.start_transaction(options);
            updateDiagnosis(retrySession, profiles, intakes, 1024, "rechecked");
            retrySession.commit_transaction();
        } catch (const mongocxx::operation_exception& error) {
            if (!error.has_error_label("TransientTransactionError")) {
                throw;
            }
            transientErrorHandled = true;
            retrySession.abort_transaction();
        }
        blockingSession.commit_transaction();

        if (!transientErrorHandled) {
            throw std::runtime_error("Временная ошибка транзакции не получена");
        }

        retrySession.start_transaction(options);
        updateDiagnosis(retrySession, profiles, intakes, 1024, "rechecked");
        retrySession.commit_transaction();

        if (profiles.count_documents(make_document(
                kvp("device_id", 20001), kvp("parameters.memory_gb", 1024))) != 1 ||
            intakes.count_documents(make_document(
                kvp("service_order_id", 20001),
                kvp("reception_parameters.screen_condition", "rechecked"))) != 1) {
            throw std::runtime_error("Проверка повтора транзакции не пройдена");
        }

        std::cout << "transaction: committed, middle_error_rolled_back, "
                  << "transient_retry=1\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

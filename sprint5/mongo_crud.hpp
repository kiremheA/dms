#pragma once

#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/oid.hpp>
#include <mongocxx/database.hpp>

#include <bsoncxx/stdx/optional.hpp>

class MongoCrud {
public:
    explicit MongoCrud(mongocxx::database& db);

    void createDeviceParameters(int deviceId, int categoryId,
                                bsoncxx::document::view parameters);
    bsoncxx::stdx::optional<bsoncxx::document::value> readDeviceParameters(int deviceId);
    bool updateDeviceParameters(int deviceId, bsoncxx::document::view parameters);
    bool deleteDeviceParameters(int deviceId);

    void createOrderIntakeParameters(int serviceOrderId,
                                     const bsoncxx::oid& deviceProfileId,
                                     bsoncxx::document::view parameters);
    bsoncxx::stdx::optional<bsoncxx::document::value>
    readOrderIntakeParameters(int serviceOrderId);
    bool updateOrderIntakeParameters(int serviceOrderId,
                                     bsoncxx::document::view parameters);
    bool deleteOrderIntakeParameters(int serviceOrderId);

private:
    mongocxx::database& db_;

    void validateParameters(int categoryId, const char* scope,
                            bsoncxx::document::view values);
};

#pragma once

#include "pg_database.hpp"
#include "mongo_crud.hpp"

#include <QMainWindow>
#include <QHash>

#include <mongocxx/database.hpp>

#include <memory>

class QPlainTextEdit;
class QTableWidget;
class QPushButton;
class QLineEdit;

enum class AppRole { Client, Dispatcher, Master, Owner };

struct Actor {
    int id = 0;
    AppRole role = AppRole::Dispatcher;
    QString name;
};

class MainWindow final : public QMainWindow {
public:
    MainWindow(PgDatabase& pg, mongocxx::database mongoDb, Actor actor);
    static bool chooseActor(PgDatabase& pg, Actor& actor);

private:
    PgDatabase& pg_;
    mongocxx::database mongoDb_;
    MongoCrud mongo_;
    Actor actor_;
    std::unique_ptr<PgDatabase> analystPg_;
    QTableWidget* orders_ = nullptr;
    QPlainTextEdit* details_ = nullptr;
    QLineEdit* orderSearch_ = nullptr;
    QHash<QString, QPushButton*> actions_;
    int selectedOrderId_ = 0;

    void buildUi();
    void updateActionStates();
    void refreshOrders();
    void showSelectedOrder();
    void showError(const QString& message);
    void showInfo(const QString& message);
    bool hasRole(AppRole role) const;
    bool isCurrentMaster(int orderId) const;
    void createOrder();
    void registerDevice();
    void assignMaster();
    void recordDiagnosis();
    void createEstimate();
    void approveEstimate();
    void addWork();
    void writeOffPart();
    void receivePart();
    void correctMovement();
    void finishRepair();
    void cancelOrder();
    void issueInvoiceAndPay();
    void issueOrder();
    void manageDeviceParameters();
    void manageIntakeParameters();
    void showOwnerReport();
    void setOrderStatus(int orderId, const QString& status, int changedBy);
    void closeCurrentAssignment(int orderId);
    double availablePart(int partId);
    double remainingPartPlan(int orderId, int partId);
    double reservedPartForOtherOrders(int orderId, int partId);
    void resumeWaitingOrders();
    void refreshInvoice(int orderId);
    int selectedOrder() const;
};

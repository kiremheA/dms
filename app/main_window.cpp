#include "main_window.hpp"

#include <bsoncxx/json.hpp>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <stdexcept>

namespace {

QString envValue(const char* key) {
    const char* value = std::getenv(key);
    return value ? QString::fromUtf8(value) : QString();
}

QString roleName(AppRole role) {
    switch (role) {
    case AppRole::Client: return "client";
    case AppRole::Dispatcher: return "dispatcher";
    case AppRole::Master: return "master";
    case AppRole::Owner: return "owner";
    }
    return {};
}

QString roleLabel(AppRole role) {
    switch (role) {
    case AppRole::Client: return "Клиент";
    case AppRole::Dispatcher: return "Диспетчер";
    case AppRole::Master: return "Мастер";
    case AppRole::Owner: return "Владелец";
    }
    return {};
}

QString statusLabel(const QString& status) {
    static const QHash<QString, QString> labels = {
        {"queued", "В очереди"}, {"diagnosis", "Диагностика"},
        {"awaiting_approval", "Ожидает согласования"}, {"in_repair", "В ремонте"},
        {"awaiting_part", "Ожидает запчасть"}, {"ready_for_pickup", "Готово к выдаче"},
        {"issued", "Выдано"}, {"cancelled", "Отменена"}
    };
    return labels.value(status, status);
}

AppRole parseStaffRole(const QString& value) {
    if (value == "master") return AppRole::Master;
    if (value == "owner") return AppRole::Owner;
    return AppRole::Dispatcher;
}

QString asString(const PgDatabase::Row& row, const QString& key,
                 const QString& fallback = QStringLiteral("—")) {
    const QString value = row.value(key);
    return value.isEmpty() ? fallback : value;
}

int intValue(const PgDatabase::Row& row, const QString& key) {
    bool ok = false;
    const int value = row.value(key).toInt(&ok);
    if (!ok) throw std::runtime_error(("Некорректное значение " + key).toStdString());
    return value;
}

QString jsonFor(const bsoncxx::document::value& value, const char* field) {
    const auto element = value.view()[field];
    if (element && element.type() == bsoncxx::type::k_document)
        return QString::fromStdString(bsoncxx::to_json(element.get_document().view()));
    return "{}";
}

QString formatMoney(double value) {
    return QString::number(value, 'f', 2);
}

}

bool MainWindow::chooseActor(PgDatabase& pg, Actor& actor) {
    QDialog dialog;
    dialog.setWindowTitle("Демонстрационный пользователь");
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Выберите пользователя и его роль для демонстрации:"));
    auto* combo = new QComboBox;
    layout->addWidget(combo);

    const auto staff = pg.query(
        "SELECT id, full_name, role FROM staff ORDER BY role, full_name");
    for (const auto& row : staff) {
        const AppRole role = parseStaffRole(row.value("role"));
        combo->addItem(QString("%1 — %2").arg(roleLabel(role), row.value("full_name")),
                       QString("staff:%1:%2:%3")
                           .arg(row.value("id"), roleName(role), row.value("full_name")));
    }
    const auto customers = pg.query("SELECT id, full_name FROM customer ORDER BY full_name");
    for (const auto& row : customers) {
        combo->addItem(QString("Клиент — %1").arg(row.value("full_name")),
                       QString("customer:%1:client:%2")
                           .arg(row.value("id"), row.value("full_name")));
    }
    if (combo->count() == 0) {
        QMessageBox::warning(&dialog, "Нет пользователей",
                             "Сначала добавьте сотрудников и клиентов в PostgreSQL.");
        return false;
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return false;

    const QStringList parts = combo->currentData().toString().split(':');
    if (parts.size() != 4) return false;
    actor.id = parts[1].toInt();
    actor.role = parts[2] == "client" ? AppRole::Client : parseStaffRole(parts[2]);
    actor.name = parts[3];
    return true;
}

MainWindow::MainWindow(PgDatabase& pg, mongocxx::database mongoDb, Actor actor)
    : pg_(pg), mongoDb_(std::move(mongoDb)), mongo_(mongoDb_), actor_(std::move(actor)) {
    buildUi();
    refreshOrders();
}

bool MainWindow::hasRole(AppRole role) const { return actor_.role == role; }

int MainWindow::selectedOrder() const { return selectedOrderId_; }

void MainWindow::buildUi() {
    setWindowTitle("Сервисный центр — заявки");
    resize(1280, 760);

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    auto* header = new QHBoxLayout;
    auto* sessionLabel = new QLabel(QString("%1: %2 ")
                                        .arg(roleLabel(actor_.role), actor_.name));
    header->addWidget(sessionLabel);
    header->addStretch();
    rootLayout->addLayout(header);

    auto* actionsLayout = new QGridLayout;
    int actionIndex = 0;
    const auto addAction = [this, actionsLayout, &actionIndex](const QString& key,
                                                               const QString& label,
                                                               auto callback) {
        auto* button = new QPushButton(label);
        actions_.insert(key, button);
        actionsLayout->addWidget(button, actionIndex / 6, actionIndex % 6);
        ++actionIndex;
        connect(button, &QPushButton::clicked, this, callback);
    };
    addAction("new", "Новая заявка", [this] { createOrder(); });
    addAction("register_device", "Клиент и устройство", [this] { registerDevice(); });
    addAction("assign", "Назначить / переназначить", [this] { assignMaster(); });
    addAction("diagnosis", "Диагностика", [this] { recordDiagnosis(); });
    addAction("estimate", "Новая версия сметы", [this] { createEstimate(); });
    addAction("approve", "Согласовать смету", [this] { approveEstimate(); });
    addAction("work", "Зафиксировать работу", [this] { addWork(); });
    addAction("writeoff", "Списать запчасть", [this] { writeOffPart(); });
    addAction("receive", "Поступление запчасти", [this] { receivePart(); });
    addAction("correct", "Корректировка движения", [this] { correctMovement(); });
    addAction("finish", "Завершить ремонт", [this] { finishRepair(); });
    addAction("cancel", "Отменить заявку", [this] { cancelOrder(); });
    addAction("invoice", "Счёт / оплата", [this] { issueInvoiceAndPay(); });
    addAction("issue", "Выдать технику", [this] { issueOrder(); });
    addAction("device_params", "Параметры устройства", [this] { manageDeviceParameters(); });
    addAction("intake_params", "Параметры приёма", [this] { manageIntakeParameters(); });
    addAction("report", "Отчёт владельца", [this] { showOwnerReport(); });
    rootLayout->addLayout(actionsLayout);

    auto* searchLayout = new QHBoxLayout;
    searchLayout->addWidget(new QLabel("Поиск заявки:"));
    orderSearch_ = new QLineEdit;
    orderSearch_->setPlaceholderText("номер, клиент или серийный номер");
    auto* searchButton = new QPushButton("Найти");
    searchLayout->addWidget(orderSearch_, 1);
    searchLayout->addWidget(searchButton);
    rootLayout->addLayout(searchLayout);
    connect(searchButton, &QPushButton::clicked, this, [this] { refreshOrders(); });
    connect(orderSearch_, &QLineEdit::returnPressed, this, [this] { refreshOrders(); });

    auto* splitter = new QSplitter(Qt::Horizontal, root);
    orders_ = new QTableWidget(0, 7, splitter);
    orders_->setHorizontalHeaderLabels(
        {"№", "Статус", "Клиент", "Серийный номер", "Категория", "Дедлайн", "Мастер"});
    orders_->setSelectionBehavior(QAbstractItemView::SelectRows);
    orders_->setSelectionMode(QAbstractItemView::SingleSelection);
    orders_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    orders_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    orders_->horizontalHeader()->setStretchLastSection(true);
    details_ = new QPlainTextEdit(splitter);
    details_->setReadOnly(true);
    details_->setPlaceholderText("Выберите заявку, чтобы увидеть её карточку и историю.");
    splitter->addWidget(orders_);
    splitter->addWidget(details_);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    rootLayout->addWidget(splitter, 1);
    setCentralWidget(root);

    connect(orders_, &QTableWidget::itemSelectionChanged, this, [this] {
        showSelectedOrder();
        updateActionStates();
    });
}

void MainWindow::refreshOrders() {
    QString base =
        "SELECT o.id, o.status, c.full_name AS customer, d.serial_number, dc.name AS category, "
        "COALESCE(to_char(o.deadline_at, 'YYYY-MM-DD HH24:MI'), '—') AS deadline, "
        "COALESCE(s.full_name, '—') AS master "
        "FROM service_order o JOIN device d ON d.id=o.device_id "
        "JOIN customer c ON c.id=d.customer_id "
        "JOIN device_category dc ON dc.id=d.device_category_id "
        "LEFT JOIN master_assignment a ON a.service_order_id=o.id AND a.ended_at IS NULL "
        "LEFT JOIN staff s ON s.id=a.master_id ";
    if (hasRole(AppRole::Master))
        base.replace("COALESCE(s.full_name, '—') AS master",
                     "CASE WHEN a.master_id=$1 THEN COALESCE(s.full_name, '—') ELSE '—' END AS master");
    QString sql = base;
    QStringList params;
    QStringList conditions;
    if (hasRole(AppRole::Client)) {
        conditions << "d.customer_id=$1";
        params << QString::number(actor_.id);
    } else if (hasRole(AppRole::Master)) {
        conditions << "EXISTS (SELECT 1 FROM master_assignment own_a "
                    "WHERE own_a.service_order_id=o.id AND own_a.master_id=$1)";
        params << QString::number(actor_.id);
    }
    const QString search = orderSearch_->text().trimmed();
    if (!search.isEmpty()) {
        const int parameterIndex = params.size() + 1;
        conditions << QString("(o.id::text ILIKE $%1 OR c.full_name ILIKE $%1 "
                              "OR d.serial_number ILIKE $%1)").arg(parameterIndex);
        params << ("%" + search + "%");
    }
    if (!conditions.isEmpty()) sql += "WHERE " + conditions.join(" AND ") + " ";
    sql += "ORDER BY o.created_at DESC, o.id DESC LIMIT 250";
    const auto rows = pg_.query(sql, params);
    orders_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QStringList columns = {"id", "status", "customer", "serial_number",
                                     "category", "deadline", "master"};
        for (int j = 0; j < columns.size(); ++j) {
            const QString display = columns[j] == "status"
                ? statusLabel(rows[i].value(columns[j])) : rows[i].value(columns[j]);
            auto* item = new QTableWidgetItem(display);
            if (j == 0) item->setData(Qt::UserRole, rows[i].value("id").toInt());
            orders_->setItem(i, j, item);
        }
    }
    if (orders_->rowCount() > 0) orders_->selectRow(0);
    else {
        selectedOrderId_ = 0;
        details_->clear();
    }
    updateActionStates();
}

void MainWindow::showSelectedOrder() {
    const int rowIndex = orders_->currentRow();
    if (rowIndex < 0 || !orders_->item(rowIndex, 0)) {
        selectedOrderId_ = 0;
        details_->clear();
        return;
    }
    selectedOrderId_ = orders_->item(rowIndex, 0)->data(Qt::UserRole).toInt();
    QString detailSql =
        "SELECT o.id,o.status,o.order_type,o.original_order_id,o.created_at,o.deadline_at,"
        "o.warranty_days,d.id AS device_id,d.serial_number,dc.id AS category_id,dc.name AS category,"
        "c.id AS customer_id,c.full_name AS customer,c.contacts,COALESCE(s.full_name,'—') AS master "
        "FROM service_order o JOIN device d ON d.id=o.device_id "
        "JOIN device_category dc ON dc.id=d.device_category_id "
        "JOIN customer c ON c.id=d.customer_id "
        "LEFT JOIN master_assignment a ON a.service_order_id=o.id AND a.ended_at IS NULL "
        "LEFT JOIN staff s ON s.id=a.master_id WHERE o.id=$1";
    QStringList detailParams{QString::number(selectedOrderId_)};
    if (hasRole(AppRole::Master)) {
        detailSql.replace("COALESCE(s.full_name,'—') AS master",
                          "CASE WHEN a.master_id=$2 THEN COALESCE(s.full_name,'—') ELSE '—' END AS master");
        detailParams << QString::number(actor_.id);
    }
    const auto rows = pg_.query(detailSql, detailParams);
    if (rows.isEmpty()) {
        details_->setPlainText("Заявка не найдена.");
        return;
    }
    const auto& order = rows.first();
    QString text = QString("Заявка №%1\nСтатус: %2\nТип: %3\nКлиент: %4 (%5)\n"
                           "Устройство: %6\nКатегория: %7\nМастер: %8\nСоздана: %9\n"
                           "Дедлайн SLA: %10\nГарантия, дней: %11\n")
        .arg(order.value("id"), statusLabel(order.value("status")), order.value("order_type"),
             order.value("customer"), order.value("contacts"), order.value("serial_number"),
             order.value("category"), order.value("master"), order.value("created_at"),
             asString(order, "deadline_at"), order.value("warranty_days"));
    if (!order.value("original_order_id").isEmpty())
        text += "Исходная заявка: " + order.value("original_order_id") + "\n";

    text += "\nНеисправности:\n";
    const auto symptoms = pg_.query(
        "SELECT description FROM malfunction WHERE service_order_id=$1 ORDER BY id",
        {QString::number(selectedOrderId_)});
    for (const auto& symptom : symptoms) text += "• " + symptom.value("description") + "\n";

    text += "\nНазначения и диагностика:\n";
    QString assignmentSql =
        "SELECT a.id,a.master_id,s.full_name,a.started_at,a.ended_at,a.reason,"
        "d.findings,r.complexity FROM master_assignment a JOIN staff s ON s.id=a.master_id "
        "LEFT JOIN diagnosis d ON d.master_assignment_id=a.id "
        "LEFT JOIN repair_norm r ON r.id=d.repair_norm_id WHERE a.service_order_id=$1 ";
    QStringList assignmentParams{QString::number(selectedOrderId_)};
    if (hasRole(AppRole::Master)) {
        assignmentSql += "AND a.master_id=$2 ";
        assignmentParams << QString::number(actor_.id);
    }
    assignmentSql += "ORDER BY a.started_at";
    const auto assignments = pg_.query(assignmentSql, assignmentParams);
    for (const auto& item : assignments) {
        text += QString("• %1: %2, %3 — %4\n")
            .arg(item.value("full_name"), item.value("started_at"),
                 asString(item, "ended_at", "текущее назначение"),
                 asString(item, "findings", "диагностика не сохранена"));
    }

    const auto estimates = pg_.query(
        "SELECT ev.id,ev.version_number,ev.total_amount,ev.status,ev.created_at "
        "FROM estimate e JOIN estimate_version ev ON ev.estimate_id=e.id "
        "WHERE e.service_order_id=$1 ORDER BY ev.version_number DESC",
        {QString::number(selectedOrderId_)});
    text += "\nВерсии сметы:\n";
    for (const auto& estimate : estimates)
        text += QString("• v%1 — %2, %3 руб. (%4)\n")
            .arg(estimate.value("version_number"), estimate.value("status"),
                 estimate.value("total_amount"), estimate.value("created_at"));

    QString workSql =
        "SELECT w.created_at,w.actual_price,ws.name,s.full_name FROM work w "
        "JOIN work_service ws ON ws.id=w.work_service_id "
        "JOIN master_assignment a ON a.id=w.master_assignment_id JOIN staff s ON s.id=a.master_id "
        "WHERE w.service_order_id=$1 ";
    QStringList workParams{QString::number(selectedOrderId_)};
    if (hasRole(AppRole::Master)) {
        workSql += "AND a.master_id=$2 ";
        workParams << QString::number(actor_.id);
    }
    workSql += "ORDER BY w.created_at";
    text += "\nВыполненные работы:\n";
    for (const auto& work : pg_.query(workSql, workParams))
        text += QString("• %1 — %2 руб., %3 (%4)\n")
            .arg(work.value("name"), work.value("actual_price"),
                 work.value("full_name"), work.value("created_at"));

    const auto movements = pg_.query(
        "SELECT p.name,m.movement_type,m.quantity,m.price_at_movement,m.created_at "
        "FROM part_movement m JOIN part p ON p.id=m.part_id "
        "WHERE m.service_order_id=$1 ORDER BY m.created_at",
        {QString::number(selectedOrderId_)});
    text += "\nСписания запчастей:\n";
    for (const auto& movement : movements)
        text += QString("• %1 — %2 × %3 руб.\n")
            .arg(movement.value("name"), movement.value("quantity"),
                 movement.value("price_at_movement"));
    if (order.value("status") == "awaiting_part") {
        text += "\nНедостающие позиции:\n";
        const auto requiredParts = pg_.query(
            "SELECT DISTINCT l.part_id,p.name FROM estimate e "
            "JOIN estimate_version v ON v.estimate_id=e.id AND v.status='agreed' "
            "JOIN estimate_line l ON l.estimate_version_id=v.id AND l.line_type='part' "
            "JOIN part p ON p.id=l.part_id WHERE e.service_order_id=$1 ORDER BY p.name",
            {QString::number(selectedOrderId_)});
        for (const auto& part : requiredParts) {
            const int partId = intValue(part, "part_id");
            const double need = remainingPartPlan(selectedOrderId_, partId);
            const double availableForOrder = availablePart(partId) -
                reservedPartForOtherOrders(selectedOrderId_, partId);
            const double shortfall = std::max(0.0, need - availableForOrder);
            if (shortfall > 0.0001)
                text += QString("• %1 — не хватает %2\n").arg(part.value("name"), formatMoney(shortfall));
        }
    }

    const auto invoices = pg_.query(
        "SELECT i.id,i.amount,i.issued_at,p.amount AS paid,p.paid_at "
        "FROM invoice i LEFT JOIN payment p ON p.invoice_id=i.id WHERE i.service_order_id=$1",
        {QString::number(selectedOrderId_)});
    text += "\nСчёт и оплата:\n";
    for (const auto& invoice : invoices)
        text += QString("• Счёт %1 — %2 руб.; оплата: %3 руб.\n")
            .arg(invoice.value("id"), invoice.value("amount"),
                 asString(invoice, "paid", "не внесена"));

    text += "\nПараметры MongoDB:\n";
    const int deviceId = intValue(order, "device_id");
    const auto profile = mongo_.readDeviceParameters(deviceId);
    if (profile)
        text += "Устройство: " + jsonFor(*profile, "parameters") + "\n";
    else
        text += "Профиль устройства не создан.\n";
    const auto intake = mongo_.readOrderIntakeParameters(selectedOrderId_);
    if (intake)
        text += "Приём: " + jsonFor(*intake, "reception_parameters") + "\n";
    else
        text += "Параметры приёма не сохранены.\n";

    text += "\nИстория статусов:\n";
    QString historySql =
        "SELECT h.status,h.changed_at,COALESCE(s.full_name,'Система') AS actor "
        "FROM order_status_history h LEFT JOIN staff s ON s.id=h.changed_by "
        "WHERE h.service_order_id=$1";
    QStringList historyParams{QString::number(selectedOrderId_)};
    if (hasRole(AppRole::Master) && !isCurrentMaster(selectedOrderId_)) {
        historySql += " AND h.changed_by=$2";
        historyParams << QString::number(actor_.id);
    }
    historySql += " ORDER BY h.changed_at,h.id";
    const auto history = pg_.query(historySql, historyParams);
    for (const auto& event : history)
        text += QString("• %1 — %2 (%3)\n")
            .arg(event.value("changed_at"), statusLabel(event.value("status")), event.value("actor"));
    details_->setPlainText(text);
}

void MainWindow::updateActionStates() {
    if (!actions_.contains("new")) return;
    QString status;
    if (selectedOrderId_ > 0) {
        const auto row = pg_.query("SELECT status FROM service_order WHERE id=$1",
                                   {QString::number(selectedOrderId_)});
        if (!row.isEmpty()) status = row.first().value("status");
    }
    const bool currentMaster = selectedOrderId_ > 0 && isCurrentMaster(selectedOrderId_);
    const bool active = status != "issued" && status != "cancelled" && !status.isEmpty();
    const auto set = [this](const QString& name, bool enabled) {
        actions_.value(name)->setEnabled(enabled);
    };
    set("new", hasRole(AppRole::Dispatcher));
    set("register_device", hasRole(AppRole::Dispatcher));
    set("assign", hasRole(AppRole::Dispatcher) && (status == "queued" || active));
    bool assignmentNeedsDiagnosis = false;
    if (currentMaster) {
        const auto rows = pg_.query(
            "SELECT a.id FROM master_assignment a WHERE a.service_order_id=$1 AND a.master_id=$2 "
            "AND a.ended_at IS NULL AND NOT EXISTS "
            "(SELECT 1 FROM diagnosis d WHERE d.master_assignment_id=a.id)",
            {QString::number(selectedOrderId_), QString::number(actor_.id)});
        assignmentNeedsDiagnosis = !rows.isEmpty();
    }
    set("diagnosis", hasRole(AppRole::Master) && currentMaster && assignmentNeedsDiagnosis &&
                      (status == "diagnosis" || status == "awaiting_approval" ||
                       status == "in_repair" || status == "awaiting_part"));
    set("estimate", (hasRole(AppRole::Master) && currentMaster &&
                      (status == "awaiting_approval" || status == "in_repair")) ||
                     ((hasRole(AppRole::Dispatcher) || hasRole(AppRole::Owner)) &&
                      status == "ready_for_pickup"));
    set("approve", hasRole(AppRole::Client) &&
                    (status == "awaiting_approval" || status == "in_repair" ||
                     status == "ready_for_pickup"));
    set("work", hasRole(AppRole::Master) && currentMaster && status == "in_repair");
    set("writeoff", hasRole(AppRole::Master) && currentMaster && status == "in_repair");
    set("receive", hasRole(AppRole::Dispatcher) || hasRole(AppRole::Owner));
    set("correct", hasRole(AppRole::Dispatcher) || hasRole(AppRole::Owner));
    set("finish", hasRole(AppRole::Master) && currentMaster && status == "in_repair");
    set("cancel", active && (hasRole(AppRole::Dispatcher) || hasRole(AppRole::Client)));
    set("invoice", (hasRole(AppRole::Dispatcher) || hasRole(AppRole::Owner)) &&
                    (status == "ready_for_pickup" || status == "cancelled"));
    set("issue", hasRole(AppRole::Dispatcher) && status == "ready_for_pickup");
    set("device_params", hasRole(AppRole::Dispatcher) && selectedOrderId_ > 0);
    set("intake_params", hasRole(AppRole::Dispatcher) && selectedOrderId_ > 0);
    set("report", hasRole(AppRole::Owner));
}

bool MainWindow::isCurrentMaster(int orderId) const {
    if (!hasRole(AppRole::Master)) return false;
    const auto rows = const_cast<PgDatabase&>(pg_).query(
        "SELECT 1 FROM master_assignment WHERE service_order_id=$1 AND master_id=$2 "
        "AND ended_at IS NULL",
        {QString::number(orderId), QString::number(actor_.id)});
    return !rows.isEmpty();
}

void MainWindow::showError(const QString& message) {
    QMessageBox::warning(this, "Операция отклонена", message);
}

void MainWindow::showInfo(const QString& message) {
    QMessageBox::information(this, "Сервисный центр", message);
}

void MainWindow::registerDevice() {
    try {
        if (!hasRole(AppRole::Dispatcher))
            throw std::runtime_error("Клиента и устройство регистрирует диспетчер.");
        bool ok = false;
        const QString customerMode = QInputDialog::getItem(
            this, "Клиент и устройство", "Клиент", {"Новый клиент", "Существующий клиент"},
            0, false, &ok);
        if (!ok) return;

        QString customerId;
        QString customerName;
        QString customerContacts;
        if (customerMode == "Существующий клиент") {
            const auto customers = pg_.query("SELECT id,full_name,contacts FROM customer ORDER BY full_name");
            if (customers.isEmpty()) throw std::runtime_error("Клиентов пока нет.");
            QStringList labels;
            QHash<QString, PgDatabase::Row> customerByLabel;
            for (const auto& customer : customers) {
                const QString label = QString("%1 — %2").arg(customer.value("full_name"), customer.value("contacts"));
                labels << label;
                customerByLabel.insert(label, customer);
            }
            const QString label = QInputDialog::getItem(this, "Клиент", "Выберите клиента",
                                                        labels, 0, false, &ok);
            if (!ok) return;
            const auto customer = customerByLabel.value(label);
            customerId = customer.value("id");
            customerName = customer.value("full_name");
            customerContacts = customer.value("contacts");
        } else {
            customerName = QInputDialog::getText(this, "Новый клиент", "ФИО:", QLineEdit::Normal, {}, &ok);
            if (!ok) return;
            customerContacts = QInputDialog::getText(this, "Новый клиент", "Контакт:", QLineEdit::Normal, {}, &ok);
            if (!ok) return;
            if (customerName.trimmed().isEmpty() || customerContacts.trimmed().isEmpty())
                throw std::runtime_error("Для нового клиента укажите ФИО и контакт.");
        }

        const auto categories = pg_.query("SELECT id,name FROM device_category ORDER BY name");
        if (categories.isEmpty()) throw std::runtime_error("Справочник категорий пуст.");
        QStringList categoryLabels;
        QHash<QString, QString> categoryByLabel;
        for (const auto& category : categories) {
            categoryLabels << category.value("name");
            categoryByLabel.insert(category.value("name"), category.value("id"));
        }
        const QString categoryName = QInputDialog::getItem(
            this, "Устройство", "Категория", categoryLabels, 0, false, &ok);
        if (!ok) return;
        const int categoryId = categoryByLabel.value(categoryName).toInt();
        const QString serial = QInputDialog::getText(
            this, "Устройство", "Серийный номер:", QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        if (serial.trimmed().isEmpty()) throw std::runtime_error("Укажите серийный номер устройства.");
        const QString parameters = QInputDialog::getMultiLineText(
            this, "Профиль MongoDB", "Постоянные параметры устройства (JSON-объект):", "{}", &ok);
        if (!ok) return;
        const auto parameterDocument = bsoncxx::from_json(parameters.toStdString());

        QString deviceId;
        bool profileCreated = false;
        pg_.exec("BEGIN");
        try {
            if (customerId.isEmpty()) {
                const auto customer = pg_.query(
                    "INSERT INTO customer(full_name,contacts) VALUES($1,$2) RETURNING id",
                    {customerName.trimmed(), customerContacts.trimmed()});
                customerId = customer.first().value("id");
            }
            const auto device = pg_.query(
                "INSERT INTO device(customer_id,device_category_id,serial_number) VALUES($1,$2,$3) RETURNING id",
                {customerId, QString::number(categoryId), serial.trimmed()});
            deviceId = device.first().value("id");
            mongo_.createDeviceParameters(deviceId.toInt(), categoryId, parameterDocument.view());
            profileCreated = true;
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            if (profileCreated) {
                try { mongo_.deleteDeviceParameters(deviceId.toInt()); } catch (...) {}
            }
            throw;
        }
        showInfo(QString("Зарегистрировано устройство %1 для клиента %2.").arg(serial, customerName));
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::createOrder() {
    try {
        if (!hasRole(AppRole::Dispatcher)) throw std::runtime_error("Заявку может создать диспетчер.");
        const auto devices = pg_.query(
            "SELECT d.id,d.device_category_id,d.serial_number,c.full_name,dc.name AS category "
            "FROM device d JOIN customer c ON c.id=d.customer_id "
            "JOIN device_category dc ON dc.id=d.device_category_id ORDER BY d.id");
        if (devices.isEmpty()) throw std::runtime_error("Сначала зарегистрируйте клиента и устройство.");
        QStringList deviceLabels;
        QHash<QString, PgDatabase::Row> deviceByLabel;
        for (const auto& device : devices) {
            const QString label = QString("%1 — %2 — %3")
                .arg(device.value("serial_number"), device.value("category"), device.value("full_name"));
            deviceLabels << label;
            deviceByLabel.insert(label, device);
        }
        bool ok = false;
        const QString deviceLabel = QInputDialog::getItem(
            this, "Новая заявка", "Устройство", deviceLabels, 0, false, &ok);
        if (!ok) return;
        const auto device = deviceByLabel.value(deviceLabel);
        const int deviceId = intValue(device, "id");
        const int categoryId = intValue(device, "device_category_id");
        const auto category = pg_.query(
            "SELECT diagnosis_days,default_warranty_days FROM device_category WHERE id=$1",
            {QString::number(categoryId)});
        const auto normCount = pg_.query(
            "SELECT count(*) AS count FROM repair_norm WHERE device_category_id=$1",
            {QString::number(categoryId)});
        if (category.isEmpty() || category.first().value("diagnosis_days").toInt() <= 0 ||
            normCount.first().value("count").toInt() == 0)
            throw std::runtime_error("Категория не готова к приёму: задайте норматив диагностики и норматив ремонта.");

        const QStringList typeLabels = {"Обычная", "Гарантийная"};
        const QString typeLabel = QInputDialog::getItem(
            this, "Тип заявки", "Тип", typeLabels, 0, false, &ok);
        if (!ok) return;
        const bool warranty = typeLabel == "Гарантийная";
        QString originalId;
        int warrantyDays = category.first().value("default_warranty_days").toInt();
        if (warranty) {
            const auto originals = pg_.query(
                "SELECT o.id FROM service_order o JOIN order_status_history h "
                "ON h.service_order_id=o.id AND h.status='issued' "
                "WHERE o.device_id=$1 AND o.order_type='regular' AND o.status='issued' "
                "AND now() <= h.changed_at + o.warranty_days * interval '1 day' "
                "ORDER BY h.changed_at LIMIT 1",
                {QString::number(deviceId)});
            if (originals.isEmpty())
                throw std::runtime_error("Нет первой выданной заявки с действующей гарантией на это устройство.");
            originalId = originals.first().value("id");
            warrantyDays = 0;
        }

        const QString symptom = QInputDialog::getText(
            this, "Неисправность", "Опишите хотя бы один симптом:", QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        if (symptom.trimmed().isEmpty()) throw std::runtime_error("Нельзя создать заявку без симптома неисправности.");

        auto profile = mongo_.readDeviceParameters(deviceId);
        if (!profile) {
            const QString profileJson = QInputDialog::getMultiLineText(
                this, "Профиль устройства MongoDB",
                "Параметры устройства (JSON-объект):", "{}", &ok);
            if (!ok) return;
            auto profileValue = bsoncxx::from_json(profileJson.toStdString());
            mongo_.createDeviceParameters(deviceId, categoryId, profileValue.view());
            profile = mongo_.readDeviceParameters(deviceId);
        }
        if (!profile) throw std::runtime_error("Профиль устройства не удалось сохранить в MongoDB.");
        if (profile->view()["category_id"].get_int32().value != categoryId)
            throw std::runtime_error("Категория MongoDB-профиля не совпадает с категорией устройства в PostgreSQL.");
        if (!mongo_.updateDeviceParameters(
                deviceId, profile->view()["parameters"].get_document().view()))
            throw std::runtime_error("Параметры устройства не прошли повторную проверку.");

        const QString intakeJson = QInputDialog::getMultiLineText(
            this, "Параметры приёма MongoDB",
            "Параметры этого приёма (JSON-объект):", "{}", &ok);
        if (!ok) return;
        auto intakeValue = bsoncxx::from_json(intakeJson.toStdString());

        int orderId = 0;
        bool intakeCreated = false;
        pg_.exec("BEGIN");
        try {
            const auto inserted = pg_.query(
                "INSERT INTO service_order(device_id,dispatcher_id,order_type,original_order_id,"
                "warranty_days,deadline_at) VALUES($1,$2,$3,NULLIF($4,'')::integer,$5,"
                "now() + $6::integer * interval '1 day') RETURNING id",
                {QString::number(deviceId), QString::number(actor_.id),
                 warranty ? "warranty" : "regular", originalId,
                 QString::number(warrantyDays), category.first().value("diagnosis_days")});
            orderId = intValue(inserted.first(), "id");
            pg_.exec("INSERT INTO malfunction(service_order_id,description) VALUES($1,$2)",
                     {QString::number(orderId), symptom.trimmed()});
            pg_.exec("INSERT INTO order_status_history(service_order_id,status,changed_by) "
                     "VALUES($1,'queued',$2)",
                     {QString::number(orderId), QString::number(actor_.id)});
            mongo_.createOrderIntakeParameters(
                orderId, profile->view()["_id"].get_oid().value, intakeValue.view());
            intakeCreated = true;
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            if (intakeCreated) {
                try { mongo_.deleteOrderIntakeParameters(orderId); } catch (...) {}
            }
            throw;
        }
        refreshOrders();
        showInfo(QString("Создана заявка №%1 со статусом «в очереди».").arg(orderId));
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::setOrderStatus(int orderId, const QString& status, int changedBy) {
    const auto currentRows = pg_.query("SELECT status FROM service_order WHERE id=$1 FOR UPDATE",
                                       {QString::number(orderId)});
    if (currentRows.isEmpty()) throw std::runtime_error("Заявка не найдена.");
    const QString current = currentRows.first().value("status");
    const QHash<QString, QStringList> transitions = {
        {"queued", {"diagnosis", "cancelled"}},
        {"diagnosis", {"awaiting_approval", "cancelled"}},
        {"awaiting_approval", {"in_repair", "awaiting_part", "cancelled"}},
        {"in_repair", {"awaiting_part", "awaiting_approval", "ready_for_pickup", "cancelled"}},
        {"awaiting_part", {"in_repair", "awaiting_approval", "cancelled"}},
        {"ready_for_pickup", {"issued"}}
    };
    if (current == status) return;
    if (!transitions.value(current).contains(status))
        throw std::runtime_error("Такой переход статуса не разрешён.");
    if (status == "diagnosis" || status == "awaiting_approval" || status == "in_repair" ||
        status == "awaiting_part") {
        const auto assignment = pg_.query(
            "SELECT 1 FROM master_assignment WHERE service_order_id=$1 AND ended_at IS NULL",
            {QString::number(orderId)});
        if (assignment.isEmpty()) throw std::runtime_error("Для этого статуса нужно текущее назначение мастера.");
    }
    pg_.exec("UPDATE service_order SET status=$2 WHERE id=$1",
             {QString::number(orderId), status});
    pg_.exec("INSERT INTO order_status_history(service_order_id,status,changed_by) "
             "VALUES($1,$2,NULLIF($3,'')::integer)",
             {QString::number(orderId), status,
              changedBy > 0 ? QString::number(changedBy) : QString()});
}

void MainWindow::closeCurrentAssignment(int orderId) {
    pg_.exec("UPDATE master_assignment SET ended_at=now() WHERE service_order_id=$1 AND ended_at IS NULL",
             {QString::number(orderId)});
}

void MainWindow::assignMaster() {
    try {
        const int orderId = selectedOrder();
        if (!hasRole(AppRole::Dispatcher) || orderId == 0)
            throw std::runtime_error("Назначение мастера доступно диспетчеру для выбранной заявки.");
        const auto order = pg_.query(
            "SELECT o.status,d.device_category_id FROM service_order o "
            "JOIN device d ON d.id=o.device_id WHERE o.id=$1",
            {QString::number(orderId)});
        if (order.isEmpty()) throw std::runtime_error("Заявка не найдена.");
        const QString status = order.first().value("status");
        if (status == "issued" || status == "cancelled" || status == "ready_for_pickup")
            throw std::runtime_error("Нельзя назначить мастера на терминальную или завершённую заявку.");
        const auto masters = pg_.query(
            "SELECT s.id,s.full_name FROM staff s JOIN master_specialization ms ON ms.staff_id=s.id "
            "WHERE s.role='master' AND ms.device_category_id=$1 "
            "AND NOT EXISTS (SELECT 1 FROM master_assignment a WHERE a.master_id=s.id AND a.ended_at IS NULL) "
            "ORDER BY s.full_name",
            {order.first().value("device_category_id")});
        if (masters.isEmpty()) throw std::runtime_error("Нет свободного мастера нужной специализации.");
        QStringList labels;
        QHash<QString, QString> idByLabel;
        for (const auto& master : masters) {
            labels << master.value("full_name");
            idByLabel.insert(master.value("full_name"), master.value("id"));
        }
        bool ok = false;
        const QString name = QInputDialog::getItem(this, "Назначение", "Свободный мастер",
                                                   labels, 0, false, &ok);
        if (!ok) return;
        QString reason;
        const auto current = pg_.query(
            "SELECT id FROM master_assignment WHERE service_order_id=$1 AND ended_at IS NULL",
            {QString::number(orderId)});
        const QString expectedAssignmentId = current.isEmpty() ? QString() : current.first().value("id");
        if (!current.isEmpty()) {
            reason = QInputDialog::getText(this, "Переназначение", "Причина переназначения:",
                                           QLineEdit::Normal, {}, &ok);
            if (!ok || reason.trimmed().isEmpty())
                throw std::runtime_error("Для переназначения укажите причину.");
        }
        pg_.exec("BEGIN");
        try {
            const auto lockedOrder = pg_.query(
                "SELECT status FROM service_order WHERE id=$1 FOR UPDATE", {QString::number(orderId)});
            if (lockedOrder.isEmpty()) throw std::runtime_error("Заявка не найдена.");
            const QString lockedStatus = lockedOrder.first().value("status");
            if (lockedStatus == "issued" || lockedStatus == "cancelled" || lockedStatus == "ready_for_pickup")
                throw std::runtime_error("Заявка уже завершена и не может быть назначена.");
            const auto again = pg_.query(
                "SELECT id FROM master_assignment WHERE service_order_id=$1 AND ended_at IS NULL",
                {QString::number(orderId)});
            const QString actualAssignmentId = again.isEmpty() ? QString() : again.first().value("id");
            if (actualAssignmentId != expectedAssignmentId)
                throw std::runtime_error("Текущее назначение изменилось. Обновите заявку и повторите действие.");
            const QString masterId = idByLabel.value(name);
            pg_.query("SELECT id FROM staff WHERE id=$1 FOR UPDATE", {masterId});
            const auto stillFree = pg_.query(
                "SELECT 1 FROM master_assignment WHERE master_id=$1 AND ended_at IS NULL",
                {masterId});
            if (!stillFree.isEmpty()) throw std::runtime_error("Мастер уже получил другое назначение.");
            if (!again.isEmpty()) {
                if (reason.trimmed().isEmpty())
                    throw std::runtime_error("Для конкурентно появившегося назначения требуется причина переназначения.");
                pg_.exec("UPDATE master_assignment SET ended_at=now(),reason=$2 WHERE id=$1",
                         {again.first().value("id"), reason.trimmed()});
            }
            pg_.exec("INSERT INTO master_assignment(service_order_id,master_id) VALUES($1,$2)",
                     {QString::number(orderId), masterId});
            if (lockedStatus == "queued") setOrderStatus(orderId, "diagnosis", actor_.id);
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::recordDiagnosis() {
    try {
        const int orderId = selectedOrder();
        if (!hasRole(AppRole::Master) || !isCurrentMaster(orderId))
            throw std::runtime_error("Диагностику может сохранить только текущий мастер.");
        const auto order = pg_.query(
            "SELECT o.status,o.created_at,d.device_category_id,dc.diagnosis_days "
            "FROM service_order o JOIN device d ON d.id=o.device_id "
            "JOIN device_category dc ON dc.id=d.device_category_id WHERE o.id=$1",
            {QString::number(orderId)});
        if (order.isEmpty()) throw std::runtime_error("Заявка не найдена.");
        const QString currentStatus = order.first().value("status");
        if (currentStatus != "diagnosis" && currentStatus != "awaiting_approval" &&
            currentStatus != "in_repair" && currentStatus != "awaiting_part")
            throw std::runtime_error("Диагностика недоступна на текущем этапе.");
        const auto assignment = pg_.query(
            "SELECT a.id FROM master_assignment a WHERE a.service_order_id=$1 AND a.master_id=$2 "
            "AND a.ended_at IS NULL AND NOT EXISTS "
            "(SELECT 1 FROM diagnosis d WHERE d.master_assignment_id=a.id)",
            {QString::number(orderId), QString::number(actor_.id)});
        if (assignment.isEmpty()) throw std::runtime_error("Это назначение уже имеет диагностику либо не является текущим.");
        const auto norms = pg_.query(
            "SELECT id,complexity,repair_days FROM repair_norm WHERE device_category_id=$1 ORDER BY complexity",
            {order.first().value("device_category_id")});
        if (norms.isEmpty()) throw std::runtime_error("Для категории не задан норматив ремонта.");
        QStringList normLabels;
        QHash<QString, PgDatabase::Row> normByLabel;
        for (const auto& norm : norms) {
            const QString label = QString("%1 — %2 дн.")
                .arg(norm.value("complexity"), norm.value("repair_days"));
            normLabels << label;
            normByLabel.insert(label, norm);
        }
        bool ok = false;
        const QString selectedNorm = QInputDialog::getItem(
            this, "Диагностика", "Сложность и норматив", normLabels, 0, false, &ok);
        if (!ok) return;
        const QString findings = QInputDialog::getMultiLineText(
            this, "Диагностика", "Результаты диагностики:", {}, &ok);
        if (!ok) return;
        if (findings.trimmed().isEmpty()) throw std::runtime_error("Укажите результаты диагностики.");

        const auto norm = normByLabel.value(selectedNorm);
        pg_.exec("BEGIN");
        try {
            const auto lockedOrder = pg_.query(
                "SELECT status FROM service_order WHERE id=$1 FOR UPDATE",
                {QString::number(orderId)});
            if (lockedOrder.isEmpty()) throw std::runtime_error("Заявка не найдена.");
            const QString lockedStatus = lockedOrder.first().value("status");
            if (lockedStatus != "diagnosis" && lockedStatus != "awaiting_approval" &&
                lockedStatus != "in_repair" && lockedStatus != "awaiting_part")
                throw std::runtime_error("Статус заявки изменился. Обновите карточку.");
            const auto lockedAssignment = pg_.query(
                "SELECT a.id FROM master_assignment a WHERE a.service_order_id=$1 AND a.master_id=$2 "
                "AND a.ended_at IS NULL AND NOT EXISTS "
                "(SELECT 1 FROM diagnosis d WHERE d.master_assignment_id=a.id)",
                {QString::number(orderId), QString::number(actor_.id)});
            if (lockedAssignment.isEmpty())
                throw std::runtime_error("Текущее назначение изменилось или уже получило диагностику.");
            pg_.exec("INSERT INTO diagnosis(service_order_id,master_assignment_id,repair_norm_id,findings) "
                     "VALUES($1,$2,$3,$4)",
                     {QString::number(orderId), lockedAssignment.first().value("id"),
                      norm.value("id"), findings.trimmed()});
            const int totalDays = order.first().value("diagnosis_days").toInt() +
                                  norm.value("repair_days").toInt();
            pg_.exec(
                "UPDATE service_order o SET deadline_at=o.created_at + $2::integer * interval '1 day' + "
                "COALESCE((SELECT sum(next_changed - changed_at) FROM ("
                "SELECT status,changed_at,lead(status) OVER(ORDER BY changed_at,id) AS next_status,"
                "lead(changed_at) OVER(ORDER BY changed_at,id) AS next_changed "
                "FROM order_status_history WHERE service_order_id=o.id) periods "
                "WHERE status='awaiting_part' AND next_status IN ('in_repair','awaiting_approval')),interval '0') "
                "WHERE o.id=$1",
                {QString::number(orderId), QString::number(totalDays)});
            if (currentStatus == "awaiting_part") {
                pg_.exec("UPDATE service_order SET deadline_at=deadline_at + "
                         "(now()-(SELECT max(changed_at) FROM order_status_history "
                         "WHERE service_order_id=$1 AND status='awaiting_part')) WHERE id=$1",
                         {QString::number(orderId)});
            }
            setOrderStatus(orderId, "awaiting_approval", actor_.id);
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::createEstimate() {
    struct Line { QString type; QString itemId; double qty; double price; };
    try {
        const int orderId = selectedOrder();
        const auto order = pg_.query(
            "SELECT o.status,o.order_type,d.device_category_id FROM service_order o "
            "JOIN device d ON d.id=o.device_id WHERE o.id=$1",
            {QString::number(orderId)});
        if (order.isEmpty()) throw std::runtime_error("Заявка не найдена.");
        const QString status = order.first().value("status");
        const bool masterAction = hasRole(AppRole::Master) && isCurrentMaster(orderId) &&
            (status == "awaiting_approval" || status == "in_repair");
        const bool disputeAction = (hasRole(AppRole::Dispatcher) || hasRole(AppRole::Owner)) &&
            status == "ready_for_pickup";
        if (!masterAction && !disputeAction)
            throw std::runtime_error("Создать версию сметы нельзя для этой роли или этапа.");

        QVector<Line> lines;
        double total = 0.0;
        bool ok = false;
        do {
            const QString typeLabel = QInputDialog::getItem(
                this, "Позиция сметы", "Тип позиции", {"Услуга", "Запчасть"}, 0, false, &ok);
            if (!ok) return;
            const bool service = typeLabel == "Услуга";
            const auto items = service
                ? pg_.query("SELECT id,name,base_price AS price FROM work_service "
                            "WHERE device_category_id=$1 ORDER BY name",
                            {order.first().value("device_category_id")})
                : pg_.query("SELECT id,name,price FROM part ORDER BY name");
            if (items.isEmpty()) throw std::runtime_error("Для этого типа позиции нет доступных записей справочника.");
            QStringList labels;
            QHash<QString, PgDatabase::Row> itemByLabel;
            for (const auto& item : items) {
                const QString label = QString("%1 — %2 руб.")
                    .arg(item.value("name"), item.value("price"));
                labels << label;
                itemByLabel.insert(label, item);
            }
            const QString selected = QInputDialog::getItem(
                this, "Позиция сметы", service ? "Услуга" : "Запчасть", labels, 0, false, &ok);
            if (!ok) return;
            const auto item = itemByLabel.value(selected);
            const double qty = QInputDialog::getDouble(
                this, "Количество", "Плановое количество:", 1.0, 0.01, 100000.0, 2, &ok);
            if (!ok) return;
            if (service && std::abs(qty - std::round(qty)) > 0.0001)
                throw std::runtime_error("Количество услуг задаётся целым числом: в таблице фактических работ нет поля количества.");
            const double price = QInputDialog::getDouble(
                this, "Цена", "Плановая цена за единицу:", item.value("price").toDouble(),
                0.0, 100000000.0, 2, &ok);
            if (!ok) return;
            lines.push_back({service ? "service" : "part", item.value("id"), qty, price});
            total += qty * price;
        } while (QMessageBox::question(this, "Смета", "Добавить ещё одну позицию?") == QMessageBox::Yes);

        const bool warranty = order.first().value("order_type") == "warranty";
        if (total < 0.0 || (!warranty && total <= 0.0))
            throw std::runtime_error("Сумма обычной сметы должна быть положительной; гарантийной — неотрицательной.");
        if (disputeAction) {
            using QtyAmount = std::pair<double, double>;
            std::map<QString, QtyAmount> expected;
            std::map<QString, QtyAmount> actual;
            for (const Line& line : lines) {
                const QString key = line.type + ":" + line.itemId;
                expected[key].first += line.qty;
                expected[key].second += line.qty * line.price;
            }
            const auto actualServices = pg_.query(
                "SELECT work_service_id,count(*) AS qty,sum(actual_price) AS amount FROM work "
                "WHERE service_order_id=$1 GROUP BY work_service_id",
                {QString::number(orderId)});
            for (const auto& item : actualServices)
                actual["service:" + item.value("work_service_id")] =
                    {item.value("qty").toDouble(), item.value("amount").toDouble()};
            const auto actualParts = pg_.query(
                "WITH RECURSIVE correction_chain(root_id,movement_id,part_id,movement_type,quantity,price_at_movement) AS ("
                "SELECT id,id,part_id,movement_type,quantity,price_at_movement FROM part_movement "
                "WHERE service_order_id=$1 AND movement_type='writeoff' UNION ALL "
                "SELECT c.root_id,m.id,m.part_id,m.movement_type,m.quantity,m.price_at_movement "
                "FROM correction_chain c JOIN part_movement m ON m.correction_of_id=c.movement_id) "
                "SELECT part_id,sum(CASE WHEN movement_type='writeoff' THEN quantity ELSE -quantity END) AS qty,"
                "sum(CASE WHEN movement_type='writeoff' THEN quantity*price_at_movement "
                "ELSE -quantity*price_at_movement END) AS amount FROM correction_chain GROUP BY part_id",
                {QString::number(orderId)});
            for (const auto& item : actualParts)
                actual["part:" + item.value("part_id")] =
                    {item.value("qty").toDouble(), item.value("amount").toDouble()};
            if (expected.size() != actual.size())
                throw std::runtime_error("Смета для спора должна содержать ровно фактически выполненные работы и списанные запчасти.");
            for (const auto& [key, expectedValue] : expected) {
                const auto it = actual.find(key);
                if (it == actual.end() || std::abs(it->second.first - expectedValue.first) > 0.001 ||
                    std::abs(it->second.second - expectedValue.second) > 0.01)
                    throw std::runtime_error("Позиция сметы для спора не совпадает с фактическим количеством или стоимостью.");
            }
        }
        pg_.exec("BEGIN");
        try {
            const auto locked = pg_.query(
                "SELECT status FROM service_order WHERE id=$1 FOR UPDATE", {QString::number(orderId)});
            if (locked.isEmpty()) throw std::runtime_error("Заявка не найдена.");
            const QString lockedStatus = locked.first().value("status");
            const bool stillMasterAction = hasRole(AppRole::Master) &&
                (lockedStatus == "awaiting_approval" || lockedStatus == "in_repair") &&
                isCurrentMaster(orderId);
            const bool stillDisputeAction =
                (hasRole(AppRole::Dispatcher) || hasRole(AppRole::Owner)) &&
                lockedStatus == "ready_for_pickup";
            if (!stillMasterAction && !stillDisputeAction)
                throw std::runtime_error("Роль или статус заявки изменились. Обновите карточку.");
            auto estimate = pg_.query("SELECT id FROM estimate WHERE service_order_id=$1",
                                      {QString::number(orderId)});
            QString estimateId;
            if (estimate.isEmpty()) {
                estimate = pg_.query("INSERT INTO estimate(service_order_id) VALUES($1) RETURNING id",
                                     {QString::number(orderId)});
            }
            estimateId = estimate.first().value("id");
            pg_.exec("UPDATE estimate_version SET status='superseded' "
                     "WHERE estimate_id=$1 AND status='draft'",
                     {estimateId});
            const auto next = pg_.query(
                "SELECT COALESCE(max(version_number),0)+1 AS n FROM estimate_version WHERE estimate_id=$1",
                {estimateId});
            const QString version = next.first().value("n");
            const auto versionRow = pg_.query(
                "INSERT INTO estimate_version(estimate_id,version_number,total_amount,status) "
                "VALUES($1,$2,$3,'draft') RETURNING id",
                {estimateId, version, formatMoney(total)});
            const QString versionId = versionRow.first().value("id");
            for (const Line& line : lines) {
                if (line.type == "service")
                    pg_.exec("INSERT INTO estimate_line(estimate_version_id,line_type,work_service_id,"
                             "planned_qty,planned_price) VALUES($1,'service',$2,$3,$4)",
                             {versionId, line.itemId, formatMoney(line.qty), formatMoney(line.price)});
                else
                    pg_.exec("INSERT INTO estimate_line(estimate_version_id,line_type,part_id,"
                             "planned_qty,planned_price) VALUES($1,'part',$2,$3,$4)",
                             {versionId, line.itemId, formatMoney(line.qty), formatMoney(line.price)});
            }
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        showInfo(QString("Сохранена новая версия сметы на %1 руб. Ожидается согласование клиента.")
                 .arg(formatMoney(total)));
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

double MainWindow::availablePart(int partId) {
    const auto rows = pg_.query(
        "SELECT COALESCE(sum(CASE WHEN movement_type='receipt' THEN quantity ELSE -quantity END),0) AS qty "
        "FROM part_movement WHERE part_id=$1", {QString::number(partId)});
    return rows.first().value("qty").toDouble();
}

double MainWindow::remainingPartPlan(int orderId, int partId) {
    const auto rows = pg_.query(
        "WITH RECURSIVE movement_chain(root_id,movement_id,part_id,movement_type,quantity) AS ("
        "SELECT id,id,part_id,movement_type,quantity FROM part_movement "
        "WHERE service_order_id=$1 AND movement_type='writeoff' UNION ALL "
        "SELECT c.root_id,m.id,m.part_id,m.movement_type,m.quantity FROM movement_chain c "
        "JOIN part_movement m ON m.correction_of_id=c.movement_id) "
        "SELECT GREATEST(0,COALESCE((SELECT sum(l.planned_qty) FROM estimate e "
        "JOIN estimate_version v ON v.estimate_id=e.id AND v.status='agreed' "
        "JOIN estimate_line l ON l.estimate_version_id=v.id AND l.line_type='part' "
        "WHERE e.service_order_id=$1 AND l.part_id=$2),0)-"
        "COALESCE((SELECT sum(CASE WHEN movement_type='writeoff' THEN quantity ELSE -quantity END) "
        "FROM movement_chain WHERE part_id=$2),0)) AS qty",
        {QString::number(orderId), QString::number(partId)});
    return rows.first().value("qty").toDouble();
}

double MainWindow::reservedPartForOtherOrders(int orderId, int partId) {
    const auto orders = pg_.query(
        "SELECT id FROM service_order WHERE status='in_repair' AND id<>$1",
        {QString::number(orderId)});
    double reserved = 0.0;
    for (const auto& order : orders)
        reserved += remainingPartPlan(intValue(order, "id"), partId);
    return reserved;
}

void MainWindow::approveEstimate() {
    try {
        const int orderId = selectedOrder();
        if (!hasRole(AppRole::Client) || orderId == 0)
            throw std::runtime_error("Согласовать смету может клиент этой заявки.");
        const auto owner = pg_.query(
            "SELECT d.customer_id,o.status FROM service_order o JOIN device d ON d.id=o.device_id "
            "WHERE o.id=$1", {QString::number(orderId)});
        if (owner.isEmpty() || owner.first().value("customer_id").toInt() != actor_.id)
            throw std::runtime_error("Можно согласовывать только собственную заявку.");
        const QString status = owner.first().value("status");
        if (status != "awaiting_approval" && status != "in_repair" && status != "ready_for_pickup")
            throw std::runtime_error("Смету нельзя согласовать на текущем этапе.");
        const auto drafts = pg_.query(
            "SELECT v.id,v.total_amount,o.order_type FROM estimate e "
            "JOIN estimate_version v ON v.estimate_id=e.id JOIN service_order o ON o.id=e.service_order_id "
            "WHERE e.service_order_id=$1 AND v.status='draft' ORDER BY v.version_number DESC LIMIT 1",
            {QString::number(orderId)});
        if (drafts.isEmpty()) throw std::runtime_error("Нет новой версии сметы для согласования.");
        const auto draftLines = pg_.query(
            "SELECT part_id,sum(planned_qty) AS planned_qty FROM estimate_line "
            "WHERE estimate_version_id=$1 AND line_type='part' GROUP BY part_id",
            {drafts.first().value("id")});
        const auto currentAgreed = pg_.query(
            "SELECT id FROM estimate_version v JOIN estimate e ON e.id=v.estimate_id "
            "WHERE e.service_order_id=$1 AND v.status='agreed'",
            {QString::number(orderId)});
        if (status == "ready_for_pickup" && currentAgreed.isEmpty())
            throw std::runtime_error("Для разрешения спора должна существовать ранее согласованная смета.");
        if (QMessageBox::question(this, "Согласование сметы",
                                 QString("Согласовать версию на %1 руб.?")
                                     .arg(drafts.first().value("total_amount"))) != QMessageBox::Yes)
            return;

        pg_.exec("BEGIN");
        try {
            const auto lockedOrder = pg_.query(
                "SELECT status FROM service_order WHERE id=$1 FOR UPDATE", {QString::number(orderId)});
            if (lockedOrder.isEmpty() || lockedOrder.first().value("status") != status)
                throw std::runtime_error("Статус заявки изменился. Обновите карточку.");
            if (status != "awaiting_approval" && status != "in_repair" && status != "ready_for_pickup")
                throw std::runtime_error("Заявку уже нельзя согласовать.");
            pg_.query("SELECT id FROM part ORDER BY id FOR UPDATE");
            pg_.exec("UPDATE estimate_version SET status='superseded' "
                     "WHERE estimate_id=(SELECT id FROM estimate WHERE service_order_id=$1) "
                     "AND status='agreed'", {QString::number(orderId)});
            pg_.exec("UPDATE estimate_version SET status='agreed',agreed_at=now() WHERE id=$1 AND status='draft'",
                     {drafts.first().value("id")});
            if (status == "awaiting_approval" || status == "in_repair") {
                bool missingPart = false;
                for (const auto& line : draftLines) {
                    const int partId = line.value("part_id").toInt();
                    const auto written = pg_.query(
                        "WITH RECURSIVE movement_chain(root_id,movement_id,part_id,movement_type,quantity) AS ("
                        "SELECT id,id,part_id,movement_type,quantity FROM part_movement "
                        "WHERE service_order_id=$1 AND movement_type='writeoff' UNION ALL "
                        "SELECT c.root_id,m.id,m.part_id,m.movement_type,m.quantity FROM movement_chain c "
                        "JOIN part_movement m ON m.correction_of_id=c.movement_id) "
                        "SELECT COALESCE(sum(CASE WHEN movement_type='writeoff' THEN quantity ELSE -quantity END),0) AS qty "
                        "FROM movement_chain WHERE part_id=$2",
                        {QString::number(orderId), QString::number(partId)});
                    const double stillNeeded = std::max(0.0,
                        line.value("planned_qty").toDouble() - written.first().value("qty").toDouble());
                    const double availableForOrder = availablePart(partId) -
                        reservedPartForOtherOrders(orderId, partId);
                    if (availableForOrder + 0.0001 < stillNeeded) {
                        missingPart = true;
                        break;
                    }
                }
                const QString nextStatus = missingPart ? "awaiting_part" : "in_repair";
                if (nextStatus != status) setOrderStatus(orderId, nextStatus, actor_.id);
            }
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        if (status == "ready_for_pickup") refreshInvoice(orderId);
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::addWork() {
    try {
        const int orderId = selectedOrder();
        if (!hasRole(AppRole::Master) || !isCurrentMaster(orderId))
            throw std::runtime_error("Работу может зафиксировать только текущий мастер.");
        const auto order = pg_.query(
            "SELECT o.status,d.device_category_id FROM service_order o "
            "JOIN device d ON d.id=o.device_id WHERE o.id=$1",
            {QString::number(orderId)});
        if (order.isEmpty() || order.first().value("status") != "in_repair")
            throw std::runtime_error("Работы фиксируются только на этапе «в ремонте».");
        const auto draft = pg_.query(
            "SELECT 1 FROM estimate e JOIN estimate_version v ON v.estimate_id=e.id "
            "WHERE e.service_order_id=$1 AND v.status='draft'",
            {QString::number(orderId)});
        if (!draft.isEmpty()) throw std::runtime_error("Сначала согласуйте новую версию сметы.");
        const auto lines = pg_.query(
            "SELECT ws.id,ws.name,sum(l.planned_qty* l.planned_price)/sum(l.planned_qty) AS agreed_price,"
            "sum(l.planned_qty) AS planned "
            "FROM estimate e JOIN estimate_version v ON v.estimate_id=e.id AND v.status='agreed' "
            "JOIN estimate_line l ON l.estimate_version_id=v.id AND l.line_type='service' "
            "JOIN work_service ws ON ws.id=l.work_service_id "
            "WHERE e.service_order_id=$1 GROUP BY ws.id,ws.name ORDER BY ws.name",
            {QString::number(orderId)});
        if (lines.isEmpty()) throw std::runtime_error("В согласованной смете нет услуг.");
        QStringList labels;
        QHash<QString, PgDatabase::Row> lineByLabel;
        for (const auto& line : lines) {
            const auto done = pg_.query(
                "SELECT count(*) AS n FROM work WHERE service_order_id=$1 AND work_service_id=$2 "
                "AND actual_price=$3::numeric",
                {QString::number(orderId), line.value("id"), line.value("agreed_price")});
            if (done.first().value("n").toDouble() + 1.0 > line.value("planned").toDouble()) continue;
            const QString label = QString("%1 — %2 руб.").arg(line.value("name"), line.value("agreed_price"));
            labels << label;
            lineByLabel.insert(label, line);
        }
        if (labels.isEmpty()) throw std::runtime_error("Все услуги из согласованной сметы уже выполнены.");
        bool ok = false;
        const QString label = QInputDialog::getItem(this, "Фактическая работа", "Услуга", labels, 0, false, &ok);
        if (!ok) return;
        const auto line = lineByLabel.value(label);
        pg_.exec("BEGIN");
        try {
            const auto locked = pg_.query("SELECT status FROM service_order WHERE id=$1 FOR UPDATE",
                                          {QString::number(orderId)});
            if (locked.isEmpty() || locked.first().value("status") != "in_repair")
                throw std::runtime_error("Статус заявки изменился. Обновите карточку.");
            const auto assignment = pg_.query(
                "SELECT id FROM master_assignment WHERE service_order_id=$1 AND master_id=$2 AND ended_at IS NULL",
                {QString::number(orderId), QString::number(actor_.id)});
            if (assignment.isEmpty()) throw std::runtime_error("Текущее назначение мастера изменилось.");
            const auto unapproved = pg_.query(
                "SELECT 1 FROM estimate e JOIN estimate_version v ON v.estimate_id=e.id "
                "WHERE e.service_order_id=$1 AND v.status='draft'",
                {QString::number(orderId)});
            if (!unapproved.isEmpty()) throw std::runtime_error("Сначала согласуйте новую версию сметы.");
            const auto done = pg_.query(
                "SELECT count(*) AS n FROM work WHERE service_order_id=$1 AND work_service_id=$2 "
                "AND actual_price=$3::numeric",
                {QString::number(orderId), line.value("id"), line.value("agreed_price")});
            if (done.first().value("n").toDouble() + 1.0 > line.value("planned").toDouble())
                throw std::runtime_error("Количество работ уже соответствует смете.");
            pg_.exec("INSERT INTO work(service_order_id,master_assignment_id,work_service_id,actual_price) "
                     "VALUES($1,$2,$3,$4)",
                     {QString::number(orderId), assignment.first().value("id"), line.value("id"),
                      line.value("agreed_price")});
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::writeOffPart() {
    try {
        const int orderId = selectedOrder();
        if (!hasRole(AppRole::Master) || !isCurrentMaster(orderId))
            throw std::runtime_error("Списание может оформить только текущий мастер.");
        const auto order = pg_.query("SELECT status FROM service_order WHERE id=$1",
                                     {QString::number(orderId)});
        if (order.isEmpty() || order.first().value("status") != "in_repair")
            throw std::runtime_error("Списание возможно только на этапе «в ремонте».");
        const auto draft = pg_.query(
            "SELECT 1 FROM estimate e JOIN estimate_version v ON v.estimate_id=e.id "
            "WHERE e.service_order_id=$1 AND v.status='draft'",
            {QString::number(orderId)});
        if (!draft.isEmpty()) throw std::runtime_error("Сначала согласуйте новую версию сметы.");
        const auto lines = pg_.query(
            "SELECT p.id,p.name,sum(l.planned_qty* l.planned_price)/sum(l.planned_qty) AS price,"
            "sum(l.planned_qty) AS planned "
            "FROM estimate e JOIN estimate_version v ON v.estimate_id=e.id AND v.status='agreed' "
            "JOIN estimate_line l ON l.estimate_version_id=v.id AND l.line_type='part' "
            "JOIN part p ON p.id=l.part_id WHERE e.service_order_id=$1 "
            "GROUP BY p.id,p.name ORDER BY p.name",
            {QString::number(orderId)});
        QStringList labels;
        QHash<QString, PgDatabase::Row> lineByLabel;
        for (const auto& line : lines) {
            if (remainingPartPlan(orderId, line.value("id").toInt()) <= 0.0001) continue;
            const QString label = QString("%1 — план %2")
                .arg(line.value("name"), line.value("planned"));
            labels << label;
            lineByLabel.insert(label, line);
        }
        if (labels.isEmpty()) throw std::runtime_error("В смете нет неиспользованных запчастей.");
        bool ok = false;
        const QString label = QInputDialog::getItem(this, "Списание", "Запчасть из сметы", labels, 0, false, &ok);
        if (!ok) return;
        const auto line = lineByLabel.value(label);
        const int partId = intValue(line, "id");
        const double remaining = remainingPartPlan(orderId, partId);
        const double quantity = QInputDialog::getDouble(
            this, "Списание", "Количество:", std::min(1.0, remaining), 0.01,
            remaining, 2, &ok);
        if (!ok) return;
        pg_.exec("BEGIN");
        try {
            const auto lockedOrder = pg_.query(
                "SELECT status FROM service_order WHERE id=$1 FOR UPDATE",
                {QString::number(orderId)});
            if (lockedOrder.isEmpty() || lockedOrder.first().value("status") != "in_repair")
                throw std::runtime_error("Статус заявки изменился. Обновите карточку.");
            const auto currentAssignment = pg_.query(
                "SELECT 1 FROM master_assignment WHERE service_order_id=$1 AND master_id=$2 AND ended_at IS NULL",
                {QString::number(orderId), QString::number(actor_.id)});
            if (currentAssignment.isEmpty()) throw std::runtime_error("Текущее назначение мастера изменилось.");
            const auto drafts = pg_.query(
                "SELECT 1 FROM estimate e JOIN estimate_version v ON v.estimate_id=e.id "
                "WHERE e.service_order_id=$1 AND v.status='draft'",
                {QString::number(orderId)});
            if (!drafts.isEmpty()) throw std::runtime_error("Сначала согласуйте новую версию сметы.");
            pg_.query("SELECT id FROM part WHERE id=$1 FOR UPDATE", {QString::number(partId)});
            const double freshRemaining = remainingPartPlan(orderId, partId);
            if (quantity > freshRemaining + 0.0001)
                throw std::runtime_error("Списание превысит согласованное количество.");
            const double available = availablePart(partId) - reservedPartForOtherOrders(orderId, partId);
            if (available + 0.0001 < quantity) {
                setOrderStatus(orderId, "awaiting_part", actor_.id);
                pg_.exec("COMMIT");
                showInfo(QString("Недостаточно запчасти. Остаток: %1, требуется: %2. Заявка переведена в ожидание.")
                         .arg(formatMoney(available), formatMoney(quantity)));
                refreshOrders();
                return;
            }
            pg_.exec("INSERT INTO part_movement(part_id,service_order_id,movement_type,quantity,price_at_movement) "
                     "VALUES($1,$2,'writeoff',$3,$4)",
                     {QString::number(partId), QString::number(orderId), formatMoney(quantity),
                      line.value("price")});
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::receivePart() {
    try {
        if (!hasRole(AppRole::Dispatcher) && !hasRole(AppRole::Owner))
            throw std::runtime_error("Поступление запчасти доступно диспетчеру и владельцу.");
        const auto parts = pg_.query("SELECT id,name,sku,price FROM part ORDER BY name");
        if (parts.isEmpty()) throw std::runtime_error("Сначала заполните справочник запчастей.");
        QStringList labels;
        QHash<QString, PgDatabase::Row> partByLabel;
        for (const auto& part : parts) {
            const QString label = QString("%1 (%2)").arg(part.value("name"), part.value("sku"));
            labels << label;
            partByLabel.insert(label, part);
        }
        bool ok = false;
        const QString label = QInputDialog::getItem(this, "Поступление", "Запчасть", labels, 0, false, &ok);
        if (!ok) return;
        const auto part = partByLabel.value(label);
        const int partId = intValue(part, "id");
        const double qty = QInputDialog::getDouble(this, "Поступление", "Количество:",
                                                    1.0, 0.01, 100000.0, 2, &ok);
        if (!ok) return;
        const double price = QInputDialog::getDouble(this, "Поступление", "Цена за единицу:",
                                                      part.value("price").toDouble(), 0.0,
                                                      100000000.0, 2, &ok);
        if (!ok) return;
        pg_.exec("BEGIN");
        try {
            pg_.query("SELECT id FROM service_order WHERE status IN ('in_repair','awaiting_part') "
                      "ORDER BY id FOR UPDATE");
            pg_.query("SELECT id FROM part ORDER BY id FOR UPDATE");
            pg_.exec("INSERT INTO part_movement(part_id,movement_type,quantity,price_at_movement) "
                     "VALUES($1,'receipt',$2,$3)",
                     {QString::number(partId), formatMoney(qty), formatMoney(price)});
            resumeWaitingOrders();
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        showInfo("Поступление зарегистрировано. Ожидающие заявки проверены по FIFO.");
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::resumeWaitingOrders() {
    const auto stockRows = pg_.query(
        "SELECT part_id,COALESCE(sum(CASE WHEN movement_type='receipt' THEN quantity ELSE -quantity END),0) AS qty "
        "FROM part_movement GROUP BY part_id");
    QHash<int, double> stock;
    for (const auto& row : stockRows) stock[intValue(row, "part_id")] = row.value("qty").toDouble();

    const auto plans = pg_.query(
        "WITH RECURSIVE relevant_orders AS ("
        "SELECT o.id,o.status,CASE WHEN o.status='awaiting_part' THEN ("
        "SELECT max(h.changed_at) FROM order_status_history h "
        "WHERE h.service_order_id=o.id AND h.status='awaiting_part') END AS wait_since "
        "FROM service_order o WHERE o.status IN ('in_repair','awaiting_part')), "
        "movement_chain(order_id,movement_id,part_id,movement_type,quantity) AS ("
        "SELECT m.service_order_id,m.id,m.part_id,m.movement_type,m.quantity "
        "FROM part_movement m JOIN relevant_orders r ON r.id=m.service_order_id "
        "WHERE m.movement_type='writeoff' UNION ALL "
        "SELECT c.order_id,m.id,m.part_id,m.movement_type,m.quantity "
        "FROM movement_chain c JOIN part_movement m ON m.correction_of_id=c.movement_id), "
        "net_writeoffs AS (SELECT order_id,part_id,sum(CASE WHEN movement_type='writeoff' "
        "THEN quantity ELSE -quantity END) AS qty FROM movement_chain GROUP BY order_id,part_id), "
        "requirements AS (SELECT e.service_order_id AS order_id,l.part_id,"
        "GREATEST(0,sum(l.planned_qty)-COALESCE(n.qty,0)) AS remaining "
        "FROM relevant_orders r JOIN estimate e ON e.service_order_id=r.id "
        "JOIN estimate_version v ON v.estimate_id=e.id AND v.status='agreed' "
        "JOIN estimate_line l ON l.estimate_version_id=v.id AND l.line_type='part' "
        "LEFT JOIN net_writeoffs n ON n.order_id=e.service_order_id AND n.part_id=l.part_id "
        "GROUP BY e.service_order_id,l.part_id,n.qty) "
        "SELECT r.id,r.status,r.wait_since,q.part_id,q.remaining "
        "FROM relevant_orders r LEFT JOIN requirements q ON q.order_id=r.id "
        "ORDER BY CASE WHEN r.status='in_repair' THEN 0 ELSE 1 END,r.wait_since,r.id,q.part_id");

    QHash<int, double> reserved;
    QHash<int, QHash<int, double>> needsByOrder;
    QHash<int, QString> waitSinceByOrder;
    QVector<int> waitingOrderIds;
    for (const auto& row : plans) {
        const int orderId = intValue(row, "id");
        if (row.value("status") == "in_repair") {
            if (!row.value("part_id").isEmpty())
                reserved[intValue(row, "part_id")] += row.value("remaining").toDouble();
            continue;
        }
        if (!needsByOrder.contains(orderId)) {
            waitingOrderIds << orderId;
            waitSinceByOrder.insert(orderId, row.value("wait_since"));
        }
        if (!row.value("part_id").isEmpty())
            needsByOrder[orderId].insert(intValue(row, "part_id"), row.value("remaining").toDouble());
    }

    QSet<int> blocked;
    for (const int orderId : waitingOrderIds) {
        bool canResume = true;
        const auto needs = needsByOrder.value(orderId);
        for (auto it = needs.cbegin(); it != needs.cend(); ++it) {
            const int partId = it.key();
            const double need = it.value();
            if (blocked.contains(partId) || stock.value(partId) - reserved.value(partId) + 0.0001 < need) {
                canResume = false;
                blocked.insert(partId);
            }
        }
        if (!canResume) continue;
        const auto current = pg_.query(
            "SELECT deadline_at FROM service_order WHERE id=$1 FOR UPDATE",
            {QString::number(orderId)});
        if (current.isEmpty()) continue;
        pg_.exec("UPDATE service_order SET deadline_at=deadline_at + "
                 "(now() - $2::timestamptz) WHERE id=$1",
                 {QString::number(orderId), waitSinceByOrder.value(orderId)});
        setOrderStatus(orderId, "in_repair", actor_.id);
        for (auto it = needs.cbegin(); it != needs.cend(); ++it)
            reserved[it.key()] += it.value();
    }
}

void MainWindow::refreshInvoice(int orderId) {
    const auto total = pg_.query(
        "WITH RECURSIVE movement_chain(root_id,movement_id,part_id,movement_type,quantity,price_at_movement) AS ("
        "SELECT id,id,part_id,movement_type,quantity,price_at_movement FROM part_movement "
        "WHERE service_order_id=$1 AND movement_type='writeoff' UNION ALL "
        "SELECT c.root_id,m.id,m.part_id,m.movement_type,m.quantity,m.price_at_movement "
        "FROM movement_chain c JOIN part_movement m ON m.correction_of_id=c.movement_id) "
        "SELECT COALESCE((SELECT sum(actual_price) FROM work WHERE service_order_id=$1),0) + "
        "COALESCE((SELECT sum(CASE WHEN movement_type='writeoff' THEN quantity*price_at_movement "
        "ELSE -quantity*price_at_movement END) FROM movement_chain),0) AS amount",
        {QString::number(orderId)});
    const QString amount = total.first().value("amount");
    const auto invoice = pg_.query("SELECT id FROM invoice WHERE service_order_id=$1",
                                   {QString::number(orderId)});
    if (invoice.isEmpty()) {
        pg_.exec("INSERT INTO invoice(service_order_id,amount) VALUES($1,$2)",
                 {QString::number(orderId), amount});
    } else {
        pg_.exec("UPDATE invoice SET amount=$2 WHERE service_order_id=$1",
                 {QString::number(orderId), amount});
    }
}

void MainWindow::finishRepair() {
    try {
        const int orderId = selectedOrder();
        if (!hasRole(AppRole::Master) || !isCurrentMaster(orderId))
            throw std::runtime_error("Завершить ремонт может только текущий мастер.");
        const auto order = pg_.query("SELECT status FROM service_order WHERE id=$1",
                                     {QString::number(orderId)});
        if (order.isEmpty() || order.first().value("status") != "in_repair")
            throw std::runtime_error("Заявка должна находиться в статусе «в ремонте».");
        const auto drafts = pg_.query(
            "SELECT 1 FROM estimate e JOIN estimate_version v ON v.estimate_id=e.id "
            "WHERE e.service_order_id=$1 AND v.status='draft'",
            {QString::number(orderId)});
        if (!drafts.isEmpty()) throw std::runtime_error("Есть несогласованная версия сметы.");
        const auto agreed = pg_.query(
            "SELECT 1 FROM estimate e JOIN estimate_version v ON v.estimate_id=e.id "
            "WHERE e.service_order_id=$1 AND v.status='agreed'",
            {QString::number(orderId)});
        if (agreed.isEmpty()) throw std::runtime_error("Сначала согласуйте смету.");
        const auto services = pg_.query(
            "SELECT DISTINCT l.work_service_id FROM estimate e "
            "JOIN estimate_version v ON v.estimate_id=e.id AND v.status='agreed' "
            "JOIN estimate_line l ON l.estimate_version_id=v.id AND l.line_type='service' "
            "WHERE e.service_order_id=$1", {QString::number(orderId)});
        for (const auto& line : services) {
            const auto planned = pg_.query(
                "SELECT sum(l.planned_qty) AS qty FROM estimate e "
                "JOIN estimate_version v ON v.estimate_id=e.id AND v.status='agreed' "
                "JOIN estimate_line l ON l.estimate_version_id=v.id AND l.line_type='service' "
                "WHERE e.service_order_id=$1 AND l.work_service_id=$2",
                {QString::number(orderId), line.value("work_service_id")});
            const auto done = pg_.query(
                "SELECT count(*) AS qty FROM work WHERE service_order_id=$1 AND work_service_id=$2",
                {QString::number(orderId), line.value("work_service_id")});
            if (done.first().value("qty").toDouble() + 0.0001 < planned.first().value("qty").toDouble())
                throw std::runtime_error("Не все работы из последней согласованной сметы зафиксированы.");
        }
        const auto parts = pg_.query(
            "SELECT DISTINCT l.part_id FROM estimate e JOIN estimate_version v ON v.estimate_id=e.id "
            "AND v.status='agreed' JOIN estimate_line l ON l.estimate_version_id=v.id "
            "AND l.line_type='part' WHERE e.service_order_id=$1", {QString::number(orderId)});
        for (const auto& line : parts) {
            const int partId = intValue(line, "part_id");
            if (remainingPartPlan(orderId, partId) > 0.0001)
                throw std::runtime_error("Не все необходимые запчасти списаны по согласованной смете.");
        }

        pg_.exec("BEGIN");
        try {
            pg_.query("SELECT id FROM service_order WHERE id=$1 FOR UPDATE", {QString::number(orderId)});
            refreshInvoice(orderId);
            closeCurrentAssignment(orderId);
            setOrderStatus(orderId, "ready_for_pickup", actor_.id);
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::cancelOrder() {
    try {
        const int orderId = selectedOrder();
        if (orderId == 0 || (!hasRole(AppRole::Dispatcher) && !hasRole(AppRole::Client)))
            throw std::runtime_error("Отменить заявку может клиент или диспетчер.");
        const auto order = pg_.query(
            "SELECT o.status,d.customer_id FROM service_order o JOIN device d ON d.id=o.device_id WHERE o.id=$1",
            {QString::number(orderId)});
        if (order.isEmpty()) throw std::runtime_error("Заявка не найдена.");
        if (hasRole(AppRole::Client) && order.first().value("customer_id").toInt() != actor_.id)
            throw std::runtime_error("Клиент может отменить только собственную заявку.");
        const QString status = order.first().value("status");
        if (status == "issued" || status == "cancelled" || status == "ready_for_pickup")
            throw std::runtime_error("Эту заявку нельзя отменить на текущем этапе.");
        if (QMessageBox::question(this, "Отмена заявки",
                                 "Перевести заявку в терминальный статус «отменена»?") != QMessageBox::Yes)
            return;
        pg_.exec("BEGIN");
        try {
            pg_.query("SELECT id FROM service_order WHERE id=$1 FOR UPDATE", {QString::number(orderId)});
            closeCurrentAssignment(orderId);
            setOrderStatus(orderId, "cancelled", hasRole(AppRole::Client) ? 0 : actor_.id);
            const auto actual = pg_.query(
                "SELECT (SELECT count(*) FROM work WHERE service_order_id=$1) + "
                "(SELECT count(*) FROM part_movement WHERE service_order_id=$1 AND movement_type='writeoff') AS n",
                {QString::number(orderId)});
            if (actual.first().value("n").toInt() > 0) refreshInvoice(orderId);
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::issueInvoiceAndPay() {
    try {
        const int orderId = selectedOrder();
        if (!hasRole(AppRole::Dispatcher) || orderId == 0)
            throw std::runtime_error("Счёт и оплату оформляет диспетчер.");
        const auto order = pg_.query("SELECT status,order_type FROM service_order WHERE id=$1",
                                     {QString::number(orderId)});
        if (order.isEmpty() || (order.first().value("status") != "ready_for_pickup" &&
                                order.first().value("status") != "cancelled"))
            throw std::runtime_error("Счёт оформляется после завершения ремонта или отмены с фактическими работами.");
        refreshInvoice(orderId);
        const auto invoice = pg_.query(
            "SELECT i.id,i.amount,p.amount AS paid FROM invoice i LEFT JOIN payment p ON p.invoice_id=i.id "
            "WHERE i.service_order_id=$1", {QString::number(orderId)});
        if (invoice.isEmpty()) throw std::runtime_error("Счёт не найден.");
        const double amount = invoice.first().value("amount").toDouble();
        if (order.first().value("order_type") == "warranty") {
            showInfo(QString("Внутренний счёт гарантийной заявки: %1 руб. Оплата не требуется.")
                     .arg(formatMoney(amount)));
            return;
        }
        if (!invoice.first().value("paid").isEmpty()) {
            showInfo("Полная оплата уже зарегистрирована.");
            return;
        }
        if (amount <= 0.0) {
            showInfo("Фактическая сумма равна нулю, платёж создавать не требуется.");
            return;
        }
        bool ok = false;
        const double payment = QInputDialog::getDouble(
            this, "Оплата", "Сумма полной оплаты:", amount, 0.01, 100000000.0, 2, &ok);
        if (!ok) return;
        if (std::abs(payment - amount) > 0.001)
            throw std::runtime_error("Обычная заявка должна быть оплачена полностью одной операцией.");
        pg_.exec("INSERT INTO payment(invoice_id,amount,received_by) VALUES($1,$2,$3)",
                 {invoice.first().value("id"), formatMoney(payment), QString::number(actor_.id)});
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::issueOrder() {
    try {
        const int orderId = selectedOrder();
        if (!hasRole(AppRole::Dispatcher) || orderId == 0)
            throw std::runtime_error("Выдачу оформляет диспетчер.");
        const auto order = pg_.query("SELECT status,order_type FROM service_order WHERE id=$1",
                                     {QString::number(orderId)});
        if (order.isEmpty() || order.first().value("status") != "ready_for_pickup")
            throw std::runtime_error("Выдать можно только заявку в статусе «готово к выдаче».");
        refreshInvoice(orderId);
        const auto draft = pg_.query(
            "SELECT 1 FROM estimate e JOIN estimate_version v ON v.estimate_id=e.id "
            "WHERE e.service_order_id=$1 AND v.status='draft'",
            {QString::number(orderId)});
        if (!draft.isEmpty()) throw std::runtime_error("До выдачи согласуйте последнюю версию сметы.");
        if (order.first().value("order_type") == "regular") {
            const auto payment = pg_.query(
                "SELECT i.amount,p.amount AS paid FROM invoice i LEFT JOIN payment p ON p.invoice_id=i.id "
                "WHERE i.service_order_id=$1", {QString::number(orderId)});
            if (payment.isEmpty() || payment.first().value("paid").isEmpty() ||
                std::abs(payment.first().value("amount").toDouble() -
                         payment.first().value("paid").toDouble()) > 0.001)
                throw std::runtime_error("Обычную заявку можно выдать только после полной оплаты счёта.");
        }
        pg_.exec("BEGIN");
        try {
            pg_.query("SELECT id FROM service_order WHERE id=$1 FOR UPDATE", {QString::number(orderId)});
            setOrderStatus(orderId, "issued", actor_.id);
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::manageDeviceParameters() {
    try {
        const int orderId = selectedOrder();
        if (!hasRole(AppRole::Dispatcher) || orderId == 0)
            throw std::runtime_error("Параметры устройства редактирует диспетчер.");
        const auto order = pg_.query(
            "SELECT d.id AS device_id,d.device_category_id FROM service_order o "
            "JOIN device d ON d.id=o.device_id WHERE o.id=$1",
            {QString::number(orderId)});
        if (order.isEmpty()) throw std::runtime_error("Устройство не найдено.");
        const int deviceId = intValue(order.first(), "device_id");
        const int categoryId = intValue(order.first(), "device_category_id");
        const auto profile = mongo_.readDeviceParameters(deviceId);
        bool ok = false;
        if (!profile) {
            const QString json = QInputDialog::getMultiLineText(
                this, "Профиль устройства", "Создать параметры устройства (JSON-объект):", "{}", &ok);
            if (!ok) return;
            const auto value = bsoncxx::from_json(json.toStdString());
            mongo_.createDeviceParameters(deviceId, categoryId, value.view());
        } else {
            const auto storedCategory = profile->view()["category_id"].get_int32().value;
            if (storedCategory != categoryId)
                throw std::runtime_error("Категория MongoDB-профиля расходится с категорией устройства в PostgreSQL.");
            const QString action = QInputDialog::getItem(
                this, "Профиль устройства", "Действие", {"Изменить", "Удалить"}, 0, false, &ok);
            if (!ok) return;
            if (action == "Удалить") {
                if (QMessageBox::question(this, "Удаление профиля",
                    "Удалить профиль? Профиль, на который ссылается приём, удалить нельзя.") != QMessageBox::Yes)
                    return;
                if (!mongo_.deleteDeviceParameters(deviceId))
                    throw std::runtime_error("Профиль не удалён.");
            } else {
                const QString json = QInputDialog::getMultiLineText(
                    this, "Профиль устройства", "Новые параметры (JSON-объект):",
                    jsonFor(*profile, "parameters"), &ok);
                if (!ok) return;
                const auto value = bsoncxx::from_json(json.toStdString());
                if (!mongo_.updateDeviceParameters(deviceId, value.view()))
                    throw std::runtime_error("Профиль не обновлён.");
            }
        }
        showSelectedOrder();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::manageIntakeParameters() {
    try {
        const int orderId = selectedOrder();
        if (!hasRole(AppRole::Dispatcher) || orderId == 0)
            throw std::runtime_error("Параметры приёма редактирует диспетчер.");
        const auto order = pg_.query(
            "SELECT d.id AS device_id,d.device_category_id FROM service_order o "
            "JOIN device d ON d.id=o.device_id WHERE o.id=$1",
            {QString::number(orderId)});
        if (order.isEmpty()) throw std::runtime_error("Заявка не найдена.");
        const int deviceId = intValue(order.first(), "device_id");
        const int categoryId = intValue(order.first(), "device_category_id");
        const auto profile = mongo_.readDeviceParameters(deviceId);
        if (!profile) throw std::runtime_error("Сначала создайте профиль устройства в MongoDB.");
        if (profile->view()["category_id"].get_int32().value != categoryId)
            throw std::runtime_error("Категория профиля MongoDB не совпадает с категорией устройства.");
        const auto intake = mongo_.readOrderIntakeParameters(orderId);
        bool ok = false;
        if (!intake) {
            const QString json = QInputDialog::getMultiLineText(
                this, "Параметры приёма", "Создать параметры приёма (JSON-объект):", "{}", &ok);
            if (!ok) return;
            const auto value = bsoncxx::from_json(json.toStdString());
            mongo_.createOrderIntakeParameters(
                orderId, profile->view()["_id"].get_oid().value, value.view());
        } else {
            if (intake->view()["device_profile_id"].get_oid().value !=
                profile->view()["_id"].get_oid().value)
                throw std::runtime_error("Параметры приёма ссылаются на другой профиль устройства.");
            const QString action = QInputDialog::getItem(
                this, "Параметры приёма", "Действие", {"Изменить", "Удалить"}, 0, false, &ok);
            if (!ok) return;
            if (action == "Удалить") {
                if (QMessageBox::question(this, "Удаление параметров",
                    "Удалить параметры приёма этой заявки?") != QMessageBox::Yes)
                    return;
                if (!mongo_.deleteOrderIntakeParameters(orderId))
                    throw std::runtime_error("Параметры приёма не удалены.");
            } else {
                const QString json = QInputDialog::getMultiLineText(
                    this, "Параметры приёма", "Новые параметры (JSON-объект):",
                    jsonFor(*intake, "reception_parameters"), &ok);
                if (!ok) return;
                const auto value = bsoncxx::from_json(json.toStdString());
                if (!mongo_.updateOrderIntakeParameters(orderId, value.view()))
                    throw std::runtime_error("Параметры приёма не обновлены.");
            }
        }
        showSelectedOrder();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::showOwnerReport() {
    try {
        if (!hasRole(AppRole::Owner))
            throw std::runtime_error("Сводная отчётность доступна только владельцу.");
        const QString conninfo = envValue("DMS_PG_ANALYST_CONNINFO");
        if (conninfo.isEmpty())
            throw std::runtime_error("Для отчётов задайте DMS_PG_ANALYST_CONNINFO с подключением app_analyst.");
        if (!analystPg_) analystPg_ = std::make_unique<PgDatabase>(conninfo);

        QDialog dialog(this);
        dialog.setWindowTitle("Отчёт владельца");
        dialog.resize(640, 480);
        auto* layout = new QVBoxLayout(&dialog);
        auto* filters = new QHBoxLayout;
        auto* fromDate = new QDateEdit(QDate::currentDate().addMonths(-1));
        auto* toDate = new QDateEdit(QDate::currentDate());
        fromDate->setCalendarPopup(true);
        toDate->setCalendarPopup(true);
        filters->addWidget(new QLabel("С:"));
        filters->addWidget(fromDate);
        filters->addWidget(new QLabel("По:"));
        filters->addWidget(toDate);
        layout->addLayout(filters);
        auto* result = new QPlainTextEdit;
        result->setReadOnly(true);
        layout->addWidget(result, 1);
        auto* close = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(close);

        const auto update = [this, result, fromDate, toDate] {
            if (fromDate->date() > toDate->date()) {
                result->setPlainText("Начало периода позже его окончания.");
                return;
            }
            const QString start = fromDate->date().toString(Qt::ISODate);
            const QString endExclusive = toDate->date().addDays(1).toString(Qt::ISODate);
            const auto active = analystPg_->query(
                "SELECT count(*) AS n FROM service_order WHERE status NOT IN ('issued','cancelled')");
            const auto overdue = analystPg_->query(
                "SELECT count(*) AS n FROM service_order WHERE status NOT IN ('issued','cancelled') "
                "AND deadline_at < now()");
            const auto revenue = analystPg_->query(
                "SELECT COALESCE(sum(amount),0) AS amount FROM payment "
                "WHERE paid_at >= $1::date AND paid_at < $2::date", {start, endExclusive});
            const auto load = analystPg_->query(
                "SELECT s.full_name,round(sum(extract(epoch FROM "
                "(least(COALESCE(a.ended_at,now()),$2::date) - greatest(a.started_at,$1::date)))) "
                "/ 3600.0,1) AS hours "
                "FROM master_assignment a JOIN staff s ON s.id=a.master_id "
                "WHERE a.started_at < $2::date AND COALESCE(a.ended_at,now()) >= $1::date "
                "GROUP BY s.id,s.full_name ORDER BY s.full_name", {start, endExclusive});
            QString text = QString("Заявок в работе: %1\nПросроченных заявок: %2\n"
                                   "Выручка за период: %3 руб.\n\nЗагрузка мастеров:\n")
                .arg(active.first().value("n"), overdue.first().value("n"),
                     revenue.first().value("amount"));
            for (const auto& master : load)
                text += QString("• %1 — %2 ч.\n").arg(master.value("full_name"), master.value("hours"));
            result->setPlainText(text);
        };
        connect(fromDate, &QDateEdit::dateChanged, &dialog, [update] { update(); });
        connect(toDate, &QDateEdit::dateChanged, &dialog, [update] { update(); });
        update();
        dialog.exec();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

void MainWindow::correctMovement() {
    try {
        if (!hasRole(AppRole::Dispatcher) && !hasRole(AppRole::Owner))
            throw std::runtime_error("Корректировку может зарегистрировать диспетчер или владелец.");
        const auto movements = pg_.query(
            "SELECT m.id,m.part_id,p.name,m.movement_type,m.quantity,m.price_at_movement,m.correction_of_id "
            "FROM part_movement m JOIN part p ON p.id=m.part_id "
            "WHERE NOT EXISTS (SELECT 1 FROM part_movement child WHERE child.correction_of_id=m.id) "
            "ORDER BY m.id DESC");
        if (movements.isEmpty()) throw std::runtime_error("Нет складских движений для корректировки.");
        QStringList labels;
        QHash<QString, PgDatabase::Row> movementByLabel;
        for (const auto& movement : movements) {
            const QString label = QString("№%1 — %2 — %3 %4")
                .arg(movement.value("id"), movement.value("name"),
                     movement.value("movement_type"), movement.value("quantity"));
            labels << label;
            movementByLabel.insert(label, movement);
        }
        bool ok = false;
        const QString label = QInputDialog::getItem(
            this, "Корректировка движения", "Исправляемое движение", labels, 0, false, &ok);
        if (!ok) return;
        const auto original = movementByLabel.value(label);
        const int partId = intValue(original, "part_id");
        const QString correctionType = original.value("movement_type") == "receipt" ? "writeoff" : "receipt";
        const QString reason = QInputDialog::getText(
            this, "Корректировка", "Причина корректировки:", QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        if (reason.trimmed().isEmpty()) throw std::runtime_error("Укажите причину корректировки.");
        const double qty = original.value("quantity").toDouble();
        pg_.exec("BEGIN");
        try {
            pg_.query("SELECT id FROM service_order WHERE status IN ('in_repair','awaiting_part') "
                      "ORDER BY id FOR UPDATE");
            pg_.query("SELECT id FROM part ORDER BY id FOR UPDATE");
            pg_.exec("INSERT INTO part_movement(part_id,movement_type,quantity,price_at_movement,"
                     "correction_of_id,correction_reason) VALUES($1,$2,$3,$4,$5,$6)",
                     {QString::number(partId), correctionType, formatMoney(qty),
                      original.value("price_at_movement"), original.value("id"), reason.trimmed()});
            if (availablePart(partId) < -0.0001)
                throw std::runtime_error("Эта корректировка создала бы отрицательный складской остаток.");
            if (correctionType == "receipt") resumeWaitingOrders();
            pg_.exec("COMMIT");
        } catch (...) {
            try { pg_.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
        showInfo("Корректирующее движение добавлено. Исходная запись сохранена без изменений.");
        refreshOrders();
    } catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}

#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"

#include <QDate>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace hospital::server {
namespace {

common::Response addInventoryInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    const QString barcode = payload.value("barcode").toString().trimmed();
    const QString drugName = payload.value("drugName").toString().trimmed();
    const QString category = payload.value("category").toString().trimmed().isEmpty()
        ? QStringLiteral("未分类")
        : payload.value("category").toString().trimmed();
    const QString specification = payload.value("specification").toString().trimmed();
    const QString unit = payload.value("unit").toString().trimmed().isEmpty()
        ? QStringLiteral("盒")
        : payload.value("unit").toString().trimmed();
    const double salePrice = payload.value("salePrice").toDouble();
    const int quantity = payload.value("quantity").toInt();
    const int warningQuantity = payload.value("warningQuantity").toInt(10);
    const QString expiryDate = payload.value("expiryDate").toString().trimmed();

    if (drugName.isEmpty() || quantity <= 0) {
        db.rollback();
        return {false, "药品名称和入库数量不能为空。", {}};
    }

    QSqlQuery query(db);
    query.prepare("SELECT id FROM drug_categories WHERE category_name = :category LIMIT 1");
    query.bindValue(":category", category);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    qint64 categoryId = 0;
    if (query.next()) {
        categoryId = query.value(0).toLongLong();
        query.finish();
    } else {
        query.finish();
        QSqlQuery insertCategory(db);
        insertCategory.prepare("INSERT INTO drug_categories (category_name, description) VALUES (:category, '')");
        insertCategory.bindValue(":category", category);
        if (!insertCategory.exec()) {
            db.rollback();
            return {false, insertCategory.lastError().text(), {}};
        }
        categoryId = insertCategory.lastInsertId().toLongLong();
    }

    QString drugLookupSql = "SELECT id, stock_quantity FROM drugs WHERE drug_name = :drug_name";
    if (!barcode.isEmpty()) {
        drugLookupSql += " OR barcode = :barcode";
    }
    drugLookupSql += " LIMIT 1";
    query.prepare(drugLookupSql);
    query.bindValue(":drug_name", drugName);
    if (!barcode.isEmpty()) {
        query.bindValue(":barcode", barcode);
    }
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    qint64 drugId = 0;
    int beforeQuantity = 0;
    if (query.next()) {
        drugId = query.value(0).toLongLong();
        beforeQuantity = query.value(1).toInt();
        query.finish();

        QSqlQuery update(db);
        update.prepare("UPDATE drugs SET stock_quantity = stock_quantity + :quantity, "
                       "barcode = COALESCE(:barcode_value, barcode), "
                       "category_id = :category_id, specification = :specification, unit = :unit, "
                       "sale_price = :sale_price, warning_quantity = :warning_quantity, expiry_date = :expiry_date "
                       "WHERE id = :id");
        update.bindValue(":quantity", quantity);
        update.bindValue(":barcode_value", barcode.isEmpty() ? QVariant() : QVariant(barcode));
        update.bindValue(":category_id", categoryId);
        update.bindValue(":specification", specification);
        update.bindValue(":unit", unit);
        update.bindValue(":sale_price", salePrice);
        update.bindValue(":warning_quantity", warningQuantity);
        update.bindValue(":expiry_date", expiryDate.isEmpty() ? QVariant() : QVariant(expiryDate));
        update.bindValue(":id", drugId);
        if (!update.exec()) {
            db.rollback();
            return {false, update.lastError().text(), {}};
        }
    } else {
        query.finish();
        QSqlQuery insertDrug(db);
        insertDrug.prepare("INSERT INTO drugs (drug_code, barcode, drug_name, category_id, specification, unit, "
                           "purchase_price, sale_price, stock_quantity, warning_quantity, expiry_date, status) "
                           "VALUES (:drug_code, :barcode, :drug_name, :category_id, :specification, :unit, "
                           "0, :sale_price, :quantity, :warning_quantity, :expiry_date, 1)");
        insertDrug.bindValue(":drug_code", "D" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));
        insertDrug.bindValue(":barcode", barcode.isEmpty() ? QVariant() : barcode);
        insertDrug.bindValue(":drug_name", drugName);
        insertDrug.bindValue(":category_id", categoryId);
        insertDrug.bindValue(":specification", specification);
        insertDrug.bindValue(":unit", unit);
        insertDrug.bindValue(":sale_price", salePrice);
        insertDrug.bindValue(":quantity", quantity);
        insertDrug.bindValue(":warning_quantity", warningQuantity);
        insertDrug.bindValue(":expiry_date", expiryDate.isEmpty() ? QVariant() : QVariant(expiryDate));
        if (!insertDrug.exec()) {
            db.rollback();
            return {false, insertDrug.lastError().text(), {}};
        }
        drugId = insertDrug.lastInsertId().toLongLong();
    }

    QSqlQuery stock(db);
    stock.prepare("INSERT INTO stock_records (drug_id, change_type, quantity, before_quantity, after_quantity, related_no, operator_id) "
                  "VALUES (:drug_id, 'IN', :quantity, :before_quantity, :after_quantity, :related_no, 1)");
    stock.bindValue(":drug_id", drugId);
    stock.bindValue(":quantity", quantity);
    stock.bindValue(":before_quantity", beforeQuantity);
    stock.bindValue(":after_quantity", beforeQuantity + quantity);
    stock.bindValue(":related_no", "IN" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));
    if (!stock.exec()) {
        db.rollback();
        return {false, stock.lastError().text(), {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    return {true, "药品入库已写入 MySQL。", {}};
}

common::Response updateInventoryInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    const QString category = payload.value("分类").toString().trimmed().isEmpty()
        ? QStringLiteral("未分类")
        : payload.value("分类").toString().trimmed();

    QSqlQuery query(db);
    query.prepare("SELECT id FROM drug_categories WHERE category_name = :category LIMIT 1");
    query.bindValue(":category", category);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    qint64 categoryId = 0;
    if (query.next()) {
        categoryId = query.value(0).toLongLong();
        query.finish();
    } else {
        query.finish();
        query.prepare("INSERT INTO drug_categories (category_name, description) VALUES (:category, '')");
        query.bindValue(":category", category);
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }
        categoryId = query.lastInsertId().toLongLong();
    }

    query.prepare("UPDATE drugs SET barcode = :barcode, drug_name = :drug_name, category_id = :category_id, "
                  "specification = :specification, unit = :unit, sale_price = :sale_price, "
                  "stock_quantity = :stock_quantity, warning_quantity = :warning_quantity, expiry_date = :expiry_date, status = :status "
                  "WHERE drug_code = :drug_code");
    query.bindValue(":barcode", payload.value("条形码").toString());
    query.bindValue(":drug_name", payload.value("药品名称").toString());
    query.bindValue(":category_id", categoryId);
    query.bindValue(":specification", payload.value("规格").toString());
    query.bindValue(":unit", payload.value("单位").toString());
    query.bindValue(":sale_price", payload.value("售价").toVariant().toDouble());
    query.bindValue(":stock_quantity", payload.value("库存").toVariant().toInt());
    query.bindValue(":warning_quantity", payload.value("预警库存").toVariant().toInt());
    const QString expiryDate = payload.value("有效期").toString().trimmed();
    query.bindValue(":expiry_date", expiryDate.isEmpty() ? QVariant() : QVariant(expiryDate));
    query.bindValue(":status", payload.value("状态").toVariant().toInt());
    query.bindValue(":drug_code", payload.value("药品编码").toString());
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    db.commit();
    return {true, "药品信息已修改。", {}};
}

common::Response disableInventoryInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare("UPDATE drugs SET status = 0 WHERE drug_code = :drug_code");
    query.bindValue(":drug_code", payload.value("药品编码").toString());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    return {true, "药品已停用。", {}};
}

} // namespace

InventoryService::InventoryService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response InventoryService::handle(const common::Request& request)
{
    if (request.action == "inbound") {
        if (m_database->isEnabled()) {
            return addInventoryInDatabase(m_database, request.payload);
        }

        const auto result = DemoRepository::instance().addInventory(request.payload);
        return {result.value("success").toBool(), result.value("message").toString(), result};
    }
    if (request.action == "update") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().updateInventory(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return updateInventoryInDatabase(m_database, request.payload);
    }
    if (request.action == "delete") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().disableInventory(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return disableInventoryInDatabase(m_database, request.payload);
    }

    if (request.action != "list") {
        return {false, "Unsupported inventory action", {}};
    }

    const QString keyword = request.payload.value("keyword").toString().trimmed();
    const QString whereSql = keyword.isEmpty()
        ? QString()
        : QString("WHERE d.drug_code LIKE CONCAT('%', :keyword_code, '%') "
                  "OR d.barcode LIKE CONCAT('%', :keyword_barcode, '%') "
                  "OR d.drug_name LIKE CONCAT('%', :keyword_name, '%') "
                  "OR c.category_name LIKE CONCAT('%', :keyword_category, '%') ");

    return SqlJson::selectRows(m_database,
        "SELECT d.drug_code AS '药品编码', d.barcode AS '条形码', d.drug_name AS '药品名称', c.category_name AS '分类', "
        "d.specification AS '规格', d.unit AS '单位', d.sale_price AS '售价', "
        "d.stock_quantity AS '库存', d.warning_quantity AS '预警库存', "
        "DATE_FORMAT(d.expiry_date, '%Y-%m-%d') AS '有效期', "
        "TRIM(BOTH '；' FROM CONCAT("
        "IF(d.stock_quantity <= d.warning_quantity, '库存不足；', ''), "
        "IF(d.expiry_date IS NOT NULL AND d.expiry_date < CURRENT_DATE, '已过期；', ''), "
        "IF(d.expiry_date IS NOT NULL AND d.expiry_date >= CURRENT_DATE AND d.expiry_date <= DATE_ADD(CURRENT_DATE, INTERVAL 30 DAY), '近30天到期；', '')"
        ")) AS '预警原因', d.status AS '状态' "
        "FROM drugs d "
        "JOIN drug_categories c ON c.id = d.category_id "
        + whereSql +
        "ORDER BY d.drug_code LIMIT 100",
        {{"keyword", keyword},
         {"keyword_code", keyword},
         {"keyword_barcode", keyword},
         {"keyword_name", keyword},
         {"keyword_category", keyword}}, "inventory");
}

} // namespace hospital::server

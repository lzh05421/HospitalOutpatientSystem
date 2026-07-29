#pragma once

#include "common/Protocol.h"

#include <QVariantMap>

namespace hospital { namespace server {

class DatabaseManager;

class SqlJson
{
public:
    static common::Response selectRows(DatabaseManager* database,
                                       const QString& sql,
                                       const QVariantMap& params = {},
                                       const QString& demoKey = {});
};

}} // namespace hospital::server

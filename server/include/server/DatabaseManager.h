#pragma once

#include "server/AppConfig.h"

#include <QSqlDatabase>
#include <QString>

namespace hospital { namespace server {

class DatabaseManager
{
public:
    bool open(const AppConfig& config);
    bool ensureOpen();
    QSqlDatabase database() const;
    bool isEnabled() const;
    bool isOpen() const;
    QString lastError() const;

private:
    void ensureCompatibilitySchema();

    AppConfig m_config;
    QSqlDatabase m_database;
    bool m_enabled = false;
    QString m_lastError;
};

}} // namespace hospital::server

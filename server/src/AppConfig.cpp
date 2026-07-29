#include "server/AppConfig.h"

#include <QSettings>

namespace hospital::server {

AppConfig AppConfig::load(const QString& filePath)
{
    AppConfig config;
    QSettings settings(filePath, QSettings::IniFormat);

    config.serverHost = settings.value("server/host", config.serverHost).toString();
    config.serverPort = settings.value("server/port", config.serverPort).toUInt();

    config.databaseEnabled = settings.value("database/enabled", config.databaseEnabled).toBool();
    config.databaseDriver = settings.value("database/driver", config.databaseDriver).toString();
    config.databaseOdbcDriver = settings.value("database/odbcDriver", config.databaseOdbcDriver).toString();
    config.databaseHost = settings.value("database/host", config.databaseHost).toString();
    config.databasePort = settings.value("database/port", config.databasePort).toInt();
    config.databaseName = settings.value("database/name", config.databaseName).toString();
    config.databaseUser = settings.value("database/user", config.databaseUser).toString();
    config.databasePassword = settings.value("database/password", config.databasePassword).toString();

    return config;
}

} // namespace hospital::server

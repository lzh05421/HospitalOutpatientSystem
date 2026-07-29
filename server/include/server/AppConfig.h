#pragma once

#include <QString>

namespace hospital { namespace server {

struct AppConfig
{
    QString serverHost = "0.0.0.0";
    quint16 serverPort = 8899;

    bool databaseEnabled = true;
    QString databaseDriver = "QODBC";
    QString databaseOdbcDriver = "MySQL ODBC 9.7 Unicode Driver";
    QString databaseHost = "127.0.0.1";
    int databasePort = 3306;
    QString databaseName = "hospital_outpatient";
    QString databaseUser = "root";
    QString databasePassword = "admin";

    static AppConfig load(const QString& filePath);
};

}} // namespace hospital::server

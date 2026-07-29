#include "client/ApiClient.h"
#include "client/EntryDialog.h"
#include "client/LoginDialog.h"
#include "client/MainWindow.h"
#include "client/PatientAppointmentWindow.h"
#include "client/PatientLoginDialog.h"
#include "client/PatientManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTcpSocket>
#include <QTimer>

using namespace hospital::client;

namespace {

QString executableName(const QString& baseName)
{
#ifdef Q_OS_WIN
    return baseName + ".exe";
#else
    return baseName;
#endif
}

QString findServerExecutable()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString exe = executableName("hospital_server");
    const QStringList candidates = {
        appDir + "/../server/" + exe,
        appDir + "/../server/Debug/" + exe,
        appDir + "/../../server/Debug/" + exe,
        appDir + "/server/" + exe
    };

    for (const QString& candidate : candidates) {
        const QFileInfo info(QDir::cleanPath(candidate));
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }

    return {};
}

void startLocalServerIfNeeded()
{
    QTcpSocket probe;
    probe.connectToHost("127.0.0.1", 8899);
    if (probe.waitForConnected(250)) {
        probe.disconnectFromHost();
        return;
    }

    const QString server = findServerExecutable();
    if (server.isEmpty()) {
        return;
    }

    const QString projectRoot = QString::fromUtf8(HOSPITAL_PROJECT_ROOT);
    const QString config = QDir(projectRoot).filePath("config/server.example.ini");
    QProcess::startDetached(server, {config}, projectRoot);
}

void connectWithRetry(ApiClient* apiClient)
{
    apiClient->connectToServer("127.0.0.1", 8899);
    QTimer::singleShot(1200, apiClient, [apiClient]() {
        if (!apiClient->isConnected()) {
            apiClient->connectToServer("127.0.0.1", 8899);
        }
    });
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    startLocalServerIfNeeded();

    ApiClient apiClient;
    connectWithRetry(&apiClient);

    EntryDialog entryDialog;
    if (entryDialog.exec() != QDialog::Accepted) {
        return 0;
    }

    if (entryDialog.choice() == EntryDialog::Choice::PatientLogin
        || entryDialog.choice() == EntryDialog::Choice::PatientAppointment) {
        PatientLoginDialog patientLoginDialog(&apiClient);
        if (patientLoginDialog.exec() != QDialog::Accepted) {
            return 0;
        }
        PatientManager patientManager(&apiClient);
        patientManager.loadPatients();
        PatientAppointmentWindow appointmentWindow(&apiClient, &patientManager);
        appointmentWindow.show();
        return app.exec();
    }

    LoginDialog loginDialog(&apiClient);
    loginDialog.setPresetUsername(entryDialog.staffUsername());
    if (loginDialog.exec() != QDialog::Accepted) {
        return 0;
    }

    MainWindow mainWindow(&apiClient);
    mainWindow.show();

    return app.exec();
}

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QThread>

namespace {

QString executableName(const QString& baseName)
{
#ifdef Q_OS_WIN
    return baseName + ".exe";
#else
    return baseName;
#endif
}

QString findExecutable(const QString& targetName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString exe = executableName(targetName);

    const QStringList candidates = {
        appDir + "/../server/" + exe,
        appDir + "/../client/" + exe,
        appDir + "/../server/Debug/" + exe,
        appDir + "/../client/Debug/" + exe,
        appDir + "/../../server/Debug/" + exe,
        appDir + "/../../client/Debug/" + exe,
        appDir + "/server/" + exe,
        appDir + "/client/" + exe
    };

    for (const QString& candidate : candidates) {
        const QFileInfo info(QDir::cleanPath(candidate));
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }

    return {};
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString projectRoot = QString::fromUtf8(HOSPITAL_PROJECT_ROOT);
    const QString server = findExecutable("hospital_server");
    const QString client = findExecutable("hospital_client");
    const QString config = QDir(projectRoot).filePath("config/server.example.ini");

    if (server.isEmpty()) {
        qCritical() << "Cannot find hospital_server executable. Please build the project first.";
        return 1;
    }

    if (client.isEmpty()) {
        qCritical() << "Cannot find hospital_client executable. Please build the project first.";
        return 1;
    }

    QProcess::startDetached(server, {config}, projectRoot);
    QThread::msleep(800);
    QProcess::startDetached(client, {}, projectRoot);

    qInfo() << "Hospital server and client started.";
    return 0;
}

#include <QFile>
#include <QSettings>
#include <QString>
#include <QtTest/QtTest>

class LinuxExampleConfigTests : public QObject
{
    Q_OBJECT

private slots:
    void exampleConfigUsesMysqlDriver();
    void linuxServerScriptsUseLinuxConfig();
    void linuxShortcutScriptCreatesDesktopLauncher();
    void linuxInstallScriptSupportsOldUbuntu();
    void linuxConfigureScriptCanUseCmake3();
    void serverCMakeAllowsOfflineRedisBuild();
};

namespace {

QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

void LinuxExampleConfigTests::exampleConfigUsesMysqlDriver()
{
    QSettings settings(QStringLiteral("config/server.linux.example.ini"), QSettings::IniFormat);
    QCOMPARE(settings.value("database/driver").toString(), QStringLiteral("QMYSQL"));
    QCOMPARE(settings.value("database/host").toString(), QStringLiteral("127.0.0.1"));
    QCOMPARE(settings.value("database/name").toString(), QStringLiteral("hospital_outpatient"));
    QCOMPARE(settings.value("database/user").toString(), QStringLiteral("root"));
}

void LinuxExampleConfigTests::linuxServerScriptsUseLinuxConfig()
{
    const QString linuxScript = readTextFile(QStringLiteral("scripts/linux_run_server.sh"));
    QVERIFY2(linuxScript.contains(QStringLiteral("config/server.linux.example.ini")),
             "linux_run_server.sh should use the Linux QMYSQL config");

    const QString kylinScript = readTextFile(QStringLiteral("scripts/kylin_run_server.sh"));
    QVERIFY2(kylinScript.contains(QStringLiteral("config/server.linux.example.ini")),
             "kylin_run_server.sh should use the Linux QMYSQL config");
}

void LinuxExampleConfigTests::linuxShortcutScriptCreatesDesktopLauncher()
{
    const QString script = readTextFile(QStringLiteral("scripts/linux_create_shortcuts.sh"));
    QVERIFY2(script.contains(QStringLiteral("Desktop")) && script.contains(QString::fromUtf8("桌面")),
             "shortcut script should support both English and Chinese desktop directories");
    QVERIFY2(script.contains(QStringLiteral("$SERVER_BINARY config/server.linux.example.ini")),
             "shortcut script should start the server with the Linux QMYSQL config");
    QVERIFY2(script.contains(QStringLiteral("hospital_client")),
             "shortcut script should create a client launcher");
    QVERIFY2(script.contains(QStringLiteral("build-kylin")),
             "shortcut script should also work after a Kylin build");
    QVERIFY2(script.contains(QStringLiteral("Type=Application")),
             "shortcut script should write .desktop launchers");
    QVERIFY2(script.contains(QStringLiteral("Terminal=true")),
             "server launcher should run in a terminal so logs stay visible");
    QVERIFY2(script.contains(QStringLiteral("Created:")),
             "shortcut script should print the exact launcher paths it created");
}

void LinuxExampleConfigTests::linuxInstallScriptSupportsOldUbuntu()
{
    const QString script = readTextFile(QStringLiteral("scripts/linux_install_deps_ubuntu.sh"));
    QVERIFY2(script.contains(QStringLiteral("qt6-base-dev")),
             "install script should try Qt6 on newer Ubuntu releases");
    QVERIFY2(script.contains(QStringLiteral("qtbase5-dev")),
             "install script should fall back to Qt5 on old Ubuntu releases");
    QVERIFY2(script.contains(QStringLiteral("cmake3")) || script.contains(QStringLiteral("cmake-data")),
             "install script should install or request a CMake 3.16+ capable package");
}

void LinuxExampleConfigTests::linuxConfigureScriptCanUseCmake3()
{
    const QString script = readTextFile(QStringLiteral("scripts/linux_configure.sh"));
    QVERIFY2(script.contains(QStringLiteral("cmake3")),
             "configure script should use cmake3 when plain cmake is too old");
    QVERIFY2(script.contains(QStringLiteral("CMAKE_COMMAND")),
             "configure script should route all configure calls through a selected CMake command");
}

void LinuxExampleConfigTests::serverCMakeAllowsOfflineRedisBuild()
{
    const QString script = readTextFile(QStringLiteral("server/CMakeLists.txt"));
    QVERIFY2(script.contains(QStringLiteral("HOSPITAL_ENABLE_REDIS")),
             "server CMake should expose a switch for offline builds without Redis");
    QVERIFY2(script.contains(QStringLiteral("Redis support disabled")),
             "server CMake should make the no-Redis build mode explicit");
}

QTEST_MAIN(LinuxExampleConfigTests)
#include "linux_example_config_tests.moc"

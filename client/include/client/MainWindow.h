#pragma once

#include <QMainWindow>
#include <QStringList>

class QListWidget;
class QLineEdit;
class QStackedWidget;

namespace hospital { namespace client {

class ApiClient;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ApiClient* apiClient, QWidget* parent = nullptr);

private:
    void addModulePage(const QString& title, QWidget* page, const QStringList& roles = {});
    bool canAccess(const QStringList& roles) const;
    void filterNavigation(const QString& keyword);

    ApiClient* m_apiClient = nullptr;
    QListWidget* m_navigation = nullptr;
    QLineEdit* m_navSearch = nullptr;
    QStackedWidget* m_pages = nullptr;
};

}} // namespace hospital::client

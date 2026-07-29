#pragma once

#include "common/Protocol.h"

#include <QJsonArray>
#include <QWidget>

class QPushButton;
class QComboBox;
class QDate;
class QDateEdit;
class QHideEvent;
class QLabel;
class QLineEdit;
class QPoint;
class QShowEvent;
class QTableWidget;
class QTimer;

namespace hospital { namespace client {

class ApiClient;

class ModulePage : public QWidget
{
    Q_OBJECT

public:
    ModulePage(const QString& title,
               const QString& description,
               const QString& module,
               const QString& action,
               ApiClient* apiClient,
               QWidget* parent = nullptr,
               int refreshIntervalMs = 0);

public slots:
    void refresh();

protected:
    ApiClient* apiClient() const;
    QTableWidget* tableWidget() const;
    void setAutoRefreshInterval(int milliseconds);
    void setSearchKeyword(const QString& keyword);
    virtual void rowsUpdated(const QJsonArray& rows);
    QJsonObject selectedRowObject() const;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onResponseReceived(const common::Response& response);
    void previousPage();
    void nextPage();
    void changePageSize(int index);
    void changeGroupFilter(int index);
    void changeDoctorFilter(int index);
    void changeClinicTypeFilter(int index);
    void changeDateFilter();
    void applySearch();
    void clearSearch();
    void exportCsv();
    void editSelectedRow();
    void deleteSelectedRow();
    void showTableContextMenu(const QPoint& position);
    void copyCurrentCell();
    void copySelectedRegistrationNo();

private:
    void requestCascadeFilterDoctors();
    void fillTable(const QJsonArray& rows);
    void renderPage();
    void updatePageLabel();
    void rebuildGroupFilter();
    QJsonArray filteredRows() const;
    QJsonArray searchedRows() const;
    QJsonArray cascadeFilterRows() const;
    bool matchesDateFilter(const QJsonObject& row) const;
    QString groupValueOf(const QJsonObject& row) const;
    QString doctorValueOf(const QJsonObject& row) const;
    QString clinicTypeValueOf(const QJsonObject& row) const;
    QDate dateValueOf(const QJsonObject& row) const;
    QString effectiveGroup() const;
    bool usesDoctorSelfScope() const;
    QString loggedInDoctorFilterName() const;
    bool usesDoctorCascadeFilter() const;
    bool usesDateFilter() const;
    bool defaultsToTodayDateFilter() const;
    QString groupTitle() const;
    QStringList groupKeys() const;
    QStringList preferredHeaders(const QJsonObject& row) const;
    QString currentCellText() const;
    QString selectedFieldText(const QString& fieldName) const;
    bool supportsCrud() const;
    bool supportsEdit() const;
    bool supportsDelete() const;
    bool supportsExport() const;
    QString exportTitle() const;
    int pageCountForRows(int rowCount) const;
    void startAutoRefreshIfVisible();

    ApiClient* m_apiClient = nullptr;
    QString m_module;
    QString m_action;
    QString m_keyword;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_prevButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QPushButton* m_searchButton = nullptr;
    QPushButton* m_clearSearchButton = nullptr;
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QComboBox* m_groupBox = nullptr;
    QLabel* m_doctorFilterLabel = nullptr;
    QComboBox* m_doctorFilterBox = nullptr;
    QLabel* m_clinicTypeFilterLabel = nullptr;
    QComboBox* m_clinicTypeFilterBox = nullptr;
    QComboBox* m_dateFilterBox = nullptr;
    QDateEdit* m_dateEdit = nullptr;
    QComboBox* m_pageSizeBox = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QLabel* m_pageLabel = nullptr;
    QTableWidget* m_table = nullptr;
    QTimer* m_refreshTimer = nullptr;
    QJsonArray m_rows;
    QJsonArray m_cascadeFilterDoctors;
    QStringList m_groupPages;
    QString m_selectedGroup;
    QString m_selectedDoctorFilter = "全部";
    QString m_selectedClinicTypeFilter = "全部";
    int m_autoRefreshIntervalMs = 0;
    int m_currentPage = 0;
    int m_pageSize = 15;
    bool m_loadedOnce = false;
};

}} // namespace hospital::client

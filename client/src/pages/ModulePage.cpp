#include "client/pages/ModulePage.h"

#include "client/ApiClient.h"

#include <QApplication>
#include <QClipboard>
#include <QDate>
#include <QDateEdit>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QFrame>
#include <QGridLayout>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QPushButton>
#include <QAbstractItemView>
#include <QList>
#include <QMenu>
#include <QSignalBlocker>
#include <QSet>
#include <QShortcut>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

namespace hospital::client {
namespace {

void tuneTable(QTableWidget* table)
{
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setMinimumSectionSize(92);
    table->horizontalHeader()->setDefaultSectionSize(132);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(40);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    table->setWordWrap(false);
    table->setTextElideMode(Qt::ElideRight);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setShowGrid(false);
    table->setMinimumHeight(280);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void resizeColumnsForReading(QTableWidget* table)
{
    if (!table || table->columnCount() == 0) {
        return;
    }

    table->resizeColumnsToContents();
    for (int column = 0; column < table->columnCount(); ++column) {
        const QString header = table->horizontalHeaderItem(column)
            ? table->horizontalHeaderItem(column)->text()
            : QString();
        int width = table->columnWidth(column) + 20;
        const int minimum = header.size() <= 3 ? 96 : 118;
        const int maximum = header.contains("明细") || header.contains("地址") || header.contains("医嘱") || header.contains("诊断")
            ? 320
            : 220;
        table->setColumnWidth(column, qBound(minimum, width, maximum));
    }
}

QPushButton* makeSecondaryButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName("secondaryButton");
    return button;
}

QPushButton* makeWarningButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName("warningButton");
    return button;
}

QPushButton* makeDangerButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName("dangerButton");
    return button;
}

QBrush urgentRowBrush()
{
    return QBrush(QColor("#fff1f2"));
}

QBrush statusTextBrush(const QString& value)
{
    if (value.contains("急诊优先") || value.contains("急诊")) {
        return QBrush(QColor("#be123c"));
    }
    if (value.contains("待叫号") || value.contains("待处理") || value.contains("待审核")) {
        return QBrush(QColor("#b45309"));
    }
    if (value.contains("已叫号") || value.contains("接诊中") || value.contains("检查中")) {
        return QBrush(QColor("#1d4ed8"));
    }
    if (value.contains("检查完成待复诊")) {
        return QBrush(QColor("#047857"));
    }
    if (value.contains("已完成") || value.contains("已接诊") || value.contains("已收费") || value.contains("已发药")) {
        return QBrush(QColor("#15803d"));
    }
    if (value.contains("退费") || value.contains("驳回") || value.contains("停用") || value.contains("取消")) {
        return QBrush(QColor("#b91c1c"));
    }
    return QBrush(QColor("#111827"));
}

void applyWorkstationCellStyle(QTableWidgetItem* item, const QString& key, const QString& value, const QJsonObject& row)
{
    if (!item) {
        return;
    }

    const bool urgent = row.value("急诊标识").toString().contains("急诊");
    if (urgent) {
        item->setBackground(urgentRowBrush());
    }

    if (key.contains("状态") || key == "急诊标识") {
        item->setForeground(statusTextBrush(value));
        QFont font = item->font();
        font.setBold(true);
        item->setFont(font);
        item->setTextAlignment(Qt::AlignCenter);
    } else if (key == "排队序号" || key == "预计等待" || key == "时段") {
        item->setTextAlignment(Qt::AlignCenter);
    } else {
        item->setForeground(QBrush(QColor("#1f2937")));
        item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    }
}

QString csvEscape(const QString& value)
{
    QString escaped = value;
    escaped.replace('"', "\"\"");
    if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n') || escaped.contains('\r')) {
        escaped = '"' + escaped + '"';
    }
    return escaped;
}

QString scheduleStatusText(const QJsonObject& row, int status)
{
    if (status == 0) {
        return "停诊";
    }
    if (status == 2 || row.value("剩余号源").toVariant().toInt() <= 0) {
        return "已满";
    }
    return "正常";
}

QString generalStatusText(int status)
{
    return status == 0 ? QString("停用") : QString("正常");
}

QString displayValueForCell(const QString& module, const QString& key, const QJsonObject& row)
{
    const QVariant rawValue = row.value(key).toVariant();
    const QString rawText = rawValue.toString();
    if (key != "状态") {
        return rawText;
    }

    bool ok = false;
    const int status = rawText.toInt(&ok);
    if (!ok) {
        return rawText;
    }

    if (module == "schedule") {
        return scheduleStatusText(row, status);
    }
    if (module == "department" || module == "doctor" || module == "inventory") {
        return generalStatusText(status);
    }
    return rawText;
}

} // namespace

ModulePage::ModulePage(const QString& title,
                       const QString& description,
                       const QString& module,
                       const QString& action,
                       ApiClient* apiClient,
                       QWidget* parent,
                       int refreshIntervalMs)
    : QWidget(parent)
    , m_apiClient(apiClient)
    , m_module(module)
    , m_action(action)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(28, 24, 28, 22);
    rootLayout->setSpacing(14);

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setObjectName("pageTitle");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* descriptionLabel = new QLabel(description, this);
    descriptionLabel->setObjectName("pageDescription");
    descriptionLabel->setWordWrap(true);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);
    auto* pageHeaderPanel = new QFrame(this);
    pageHeaderPanel->setObjectName("pageHeaderPanel");
    auto* pageHeaderLayout = new QHBoxLayout(pageHeaderPanel);
    pageHeaderLayout->setContentsMargins(18, 16, 18, 16);
    pageHeaderLayout->setSpacing(12);
    auto* titleBlock = new QVBoxLayout();
    titleBlock->setSpacing(4);
    auto* modulePill = new QLabel("门诊工作站", pageHeaderPanel);
    modulePill->setObjectName("modulePill");
    titleBlock->addWidget(titleLabel);
    titleBlock->addWidget(descriptionLabel);
    m_refreshButton = new QPushButton("刷新", this);
    m_refreshButton->setObjectName("primaryButton");
    pageHeaderLayout->addLayout(titleBlock, 1);
    pageHeaderLayout->addWidget(modulePill, 0, Qt::AlignTop);
    pageHeaderLayout->addWidget(m_refreshButton, 0, Qt::AlignTop);
    headerLayout->addWidget(pageHeaderPanel);

    auto* filterPanel = new QFrame(this);
    filterPanel->setObjectName("filterPanel");
    auto* toolbar = new QGridLayout(filterPanel);
    toolbar->setContentsMargins(14, 12, 14, 12);
    toolbar->setHorizontalSpacing(10);
    toolbar->setVerticalSpacing(10);

    auto* groupLabel = new QLabel(groupTitle(), this);
    groupLabel->setObjectName("fieldLabel");
    m_groupBox = new QComboBox(this);
    m_groupBox->setMinimumWidth(140);
    m_selectedGroup = "全部";
    m_groupBox->addItem("全部");
    m_groupBox->setCurrentText(m_selectedGroup);
    if (usesDoctorCascadeFilter()) {
        m_doctorFilterLabel = new QLabel("医生", this);
        m_doctorFilterLabel->setObjectName("fieldLabel");
        m_doctorFilterBox = new QComboBox(this);
        m_doctorFilterBox->setMinimumWidth(140);
        m_doctorFilterBox->addItem("全部");
        m_clinicTypeFilterLabel = new QLabel("号别", this);
        m_clinicTypeFilterLabel->setObjectName("fieldLabel");
        m_clinicTypeFilterBox = new QComboBox(this);
        m_clinicTypeFilterBox->setMinimumWidth(116);
        m_clinicTypeFilterBox->addItem("全部");
    }
    if (usesDateFilter()) {
        m_dateFilterBox = new QComboBox(this);
        m_dateFilterBox->setMinimumWidth(116);
        m_dateFilterBox->addItems({"全部日期", "今天", "本周", "本月", "指定日期"});
        if (defaultsToTodayDateFilter()) {
            m_dateFilterBox->setCurrentText("今天");
        }
        m_dateEdit = new QDateEdit(QDate::currentDate(), this);
        m_dateEdit->setCalendarPopup(true);
        m_dateEdit->setDisplayFormat("yyyy-MM-dd");
        m_dateEdit->setVisible(false);
    }
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("按姓名、科室、状态、编号等关键字查询");
    m_searchEdit->setMinimumWidth(260);
    m_searchButton = new QPushButton("查询", this);
    m_searchButton->setObjectName("primaryButton");
    m_clearSearchButton = makeSecondaryButton("清空", this);
    QLabel* autoRefreshLabel = nullptr;
    if (refreshIntervalMs > 0) {
        autoRefreshLabel = new QLabel(QString("自动刷新：%1秒").arg(refreshIntervalMs / 1000), this);
        autoRefreshLabel->setObjectName("hintText");
    }

    int column = 0;
    toolbar->addWidget(groupLabel, 0, column++);
    toolbar->addWidget(m_groupBox, 0, column++);
    if (m_doctorFilterLabel && m_doctorFilterBox) {
        toolbar->addWidget(m_doctorFilterLabel, 0, column++);
        toolbar->addWidget(m_doctorFilterBox, 0, column++);
    }
    if (m_clinicTypeFilterLabel && m_clinicTypeFilterBox) {
        toolbar->addWidget(m_clinicTypeFilterLabel, 0, column++);
        toolbar->addWidget(m_clinicTypeFilterBox, 0, column++);
    }
    if (m_dateFilterBox && m_dateEdit) {
        auto* dateLabel = new QLabel("日期", this);
        dateLabel->setObjectName("fieldLabel");
        toolbar->addWidget(dateLabel, 0, column++);
        toolbar->addWidget(m_dateFilterBox, 0, column++);
        toolbar->addWidget(m_dateEdit, 0, column++);
    }
    toolbar->addWidget(m_searchEdit, 0, column++, 1, 2);
    toolbar->setColumnStretch(column - 1, 1);
    toolbar->addWidget(m_searchButton, 0, column++);
    toolbar->addWidget(m_clearSearchButton, 0, column++);

    column = 0;
    if (supportsCrud()) {
        m_editButton = makeSecondaryButton("修改选中", this);
        m_deleteButton = makeDangerButton("删除/停用", this);
        m_editButton->setVisible(supportsEdit());
        m_deleteButton->setVisible(supportsDelete());
        toolbar->addWidget(m_editButton, 1, column++);
        toolbar->addWidget(m_deleteButton, 1, column++);
    }
    if (supportsExport()) {
        m_exportButton = makeSecondaryButton("导出CSV", this);
        toolbar->addWidget(m_exportButton, 1, column++);
    }
    if (autoRefreshLabel) {
        toolbar->addWidget(autoRefreshLabel, 1, column++, 1, 2);
    }
    toolbar->setColumnStretch(column, 1);

    m_table = new QTableWidget(this);
    tuneTable(m_table);

    auto* tableShell = new QFrame(this);
    tableShell->setObjectName("tableShell");
    auto* tableShellLayout = new QVBoxLayout(tableShell);
    tableShellLayout->setContentsMargins(12, 12, 12, 12);
    tableShellLayout->setSpacing(10);
    auto* tableToolbar = new QFrame(tableShell);
    tableToolbar->setObjectName("tableToolbar");
    auto* tableToolbarLayout = new QHBoxLayout(tableToolbar);
    tableToolbarLayout->setContentsMargins(12, 8, 12, 8);
    tableToolbarLayout->setSpacing(10);
    auto* tableTitle = new QLabel("数据列表", tableToolbar);
    tableTitle->setObjectName("tableTitle");
    auto* emptyStateLabel = new QLabel("暂无数据时请调整筛选条件或点击刷新", tableToolbar);
    emptyStateLabel->setObjectName("emptyStateLabel");
    tableToolbarLayout->addWidget(tableTitle);
    tableToolbarLayout->addStretch();
    tableToolbarLayout->addWidget(emptyStateLabel);
    tableShellLayout->addWidget(tableToolbar);
    tableShellLayout->addWidget(m_table, 1);
    const auto createTableToolbar = []() {};
    Q_UNUSED(createTableToolbar);

    auto* pager = new QHBoxLayout();
    pager->setSpacing(10);
    m_prevButton = makeSecondaryButton("上一页", this);
    m_nextButton = makeSecondaryButton("下一页", this);
    m_pageSizeBox = new QComboBox(this);
    m_pageSizeBox->addItems({"10 条/页", "15 条/页", "30 条/页"});
    m_pageSizeBox->setCurrentIndex(1);
    m_pageLabel = new QLabel("第 0/0 页，共 0 条", this);
    m_pageLabel->setObjectName("hintText");
    pager->addWidget(m_pageLabel);
    pager->addStretch();
    pager->addWidget(m_pageSizeBox);
    pager->addWidget(m_prevButton);
    pager->addWidget(m_nextButton);

    rootLayout->addLayout(headerLayout);
    rootLayout->addWidget(filterPanel, 0);
    rootLayout->addWidget(tableShell, 10);
    rootLayout->addLayout(pager);

    setStyleSheet(R"(
        QFrame#pageHeaderPanel {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8fbfb, stop:1 #eef8f6);
            border: 1px solid #d7e4e2;
            border-radius: 14px;
        }
        QLabel#pageTitle {
            color: #0f172a;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#pageDescription {
            color: #667085;
            font-size: 13px;
        }
        QLabel#fieldLabel {
            color: #344054;
            font-weight: 600;
        }
        QLabel#hintText {
            color: #667085;
            font-size: 12px;
        }
        QLabel#modulePill {
            color: #0f766e;
            background: #dff7f3;
            border: 1px solid #99f6e4;
            border-radius: 999px;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: 700;
        }
        QFrame#filterPanel {
            background: #ffffff;
            border: 1px solid #d7e4e2;
            border-radius: 12px;
            border-left: 4px solid #0f766e;
        }
        QFrame#tableShell {
            background: #ffffff;
            border: 1px solid #d7e4e2;
            border-radius: 14px;
        }
        QFrame#tableToolbar {
            background: #f7fbfb;
            border: 1px solid #e0ece9;
            border-radius: 10px;
        }
        QLabel#tableTitle {
            color: #0f172a;
            font-size: 14px;
            font-weight: 700;
        }
        QLabel#emptyStateLabel {
            color: #6b7b88;
            font-size: 12px;
        }
    )");

    connect(m_refreshButton, &QPushButton::clicked, this, &ModulePage::refresh);
    connect(m_prevButton, &QPushButton::clicked, this, &ModulePage::previousPage);
    connect(m_nextButton, &QPushButton::clicked, this, &ModulePage::nextPage);
    connect(m_groupBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModulePage::changeGroupFilter);
    if (m_doctorFilterBox) {
        connect(m_doctorFilterBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModulePage::changeDoctorFilter);
    }
    if (m_clinicTypeFilterBox) {
        connect(m_clinicTypeFilterBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModulePage::changeClinicTypeFilter);
    }
    if (m_dateFilterBox && m_dateEdit) {
        connect(m_dateFilterBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModulePage::changeDateFilter);
        connect(m_dateEdit, &QDateEdit::dateChanged, this, &ModulePage::changeDateFilter);
    }
    connect(m_pageSizeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModulePage::changePageSize);
    connect(m_searchButton, &QPushButton::clicked, this, &ModulePage::applySearch);
    connect(m_clearSearchButton, &QPushButton::clicked, this, &ModulePage::clearSearch);
    if (m_exportButton) {
        connect(m_exportButton, &QPushButton::clicked, this, &ModulePage::exportCsv);
    }
    if (m_editButton && m_deleteButton) {
        connect(m_editButton, &QPushButton::clicked, this, &ModulePage::editSelectedRow);
        connect(m_deleteButton, &QPushButton::clicked, this, &ModulePage::deleteSelectedRow);
    }
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &ModulePage::applySearch);
    connect(m_apiClient, &ApiClient::responseReceived, this, &ModulePage::onResponseReceived);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &ModulePage::showTableContextMenu);

    auto* copyShortcut = new QShortcut(QKeySequence::Copy, m_table);
    copyShortcut->setContext(Qt::WidgetShortcut);
    connect(copyShortcut, &QShortcut::activated, this, &ModulePage::copyCurrentCell);

    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &ModulePage::refresh);
    setAutoRefreshInterval(refreshIntervalMs);
}

ApiClient* ModulePage::apiClient() const
{
    return m_apiClient;
}

QTableWidget* ModulePage::tableWidget() const
{
    return m_table;
}

void ModulePage::setAutoRefreshInterval(int milliseconds)
{
    m_autoRefreshIntervalMs = milliseconds;
    if (!m_refreshTimer) {
        return;
    }

    if (milliseconds <= 0) {
        m_refreshTimer->stop();
        return;
    }

    startAutoRefreshIfVisible();
}

void ModulePage::setSearchKeyword(const QString& keyword)
{
    m_keyword = keyword.trimmed();
    if (m_searchEdit && m_searchEdit->text() != m_keyword) {
        m_searchEdit->setText(m_keyword);
    }
}

void ModulePage::rowsUpdated(const QJsonArray&)
{
}

void ModulePage::refresh()
{
    common::Request request;
    request.module = m_module;
    request.action = m_action;
    if (!m_keyword.isEmpty()) {
        request.payload["keyword"] = m_keyword;
    }
    if (usesDoctorCascadeFilter()) {
        const QString group = effectiveGroup();
        if (!group.isEmpty()) {
            request.payload["departmentFilter"] = group;
        }
        if (usesDoctorSelfScope()) {
            const QString doctorName = loggedInDoctorFilterName();
            if (!doctorName.isEmpty()) {
                m_selectedDoctorFilter = doctorName;
                request.payload["doctorFilter"] = doctorName;
            }
        } else if (m_selectedDoctorFilter != "全部") {
            request.payload["doctorFilter"] = m_selectedDoctorFilter;
        }
        if (m_selectedClinicTypeFilter != "全部") {
            request.payload["clinicTypeFilter"] = m_selectedClinicTypeFilter;
        }
    }
    if (m_dateFilterBox && m_dateEdit) {
        request.payload["dateFilter"] = m_dateFilterBox->currentText();
        request.payload["dateValue"] = m_dateEdit->date().toString("yyyy-MM-dd");
    }
    m_apiClient->send(request);
}

void ModulePage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!m_loadedOnce) {
        requestCascadeFilterDoctors();
    }
    refresh();
    m_loadedOnce = true;
    startAutoRefreshIfVisible();
}

void ModulePage::hideEvent(QHideEvent* event)
{
    if (m_refreshTimer) {
        m_refreshTimer->stop();
    }
    QWidget::hideEvent(event);
}

void ModulePage::startAutoRefreshIfVisible()
{
    if (!m_refreshTimer || m_autoRefreshIntervalMs <= 0) {
        return;
    }
    if (isVisible()) {
        m_refreshTimer->start(m_autoRefreshIntervalMs);
    }
}

void ModulePage::onResponseReceived(const common::Response& response)
{
    if (usesDoctorCascadeFilter()
        && response.data.value("module").toString() == "doctor"
        && response.data.value("action").toString() == "list") {
        if (response.success && response.data.contains("rows")) {
            m_cascadeFilterDoctors = response.data.value("rows").toArray();
            rebuildGroupFilter();
            m_currentPage = 0;
            renderPage();
        }
        return;
    }

    if (response.data.value("module").toString() != m_module) {
        return;
    }

    const QString responseAction = response.data.value("action").toString();
    if (m_module == "schedule"
        && (responseAction == "rulesList" || responseAction == "rulesSaveAll" || responseAction == "rangeList")) {
        return;
    }

    if (!response.success) {
        QMessageBox::warning(this, "操作失败", response.message.isEmpty() ? "服务端返回失败，请检查服务端和数据库连接。" : response.message);
        return;
    }

    if (responseAction == m_action) {
        if (response.data.contains("rows")) {
            fillTable(response.data.value("rows").toArray());
        }
        return;
    }

    const QString message = response.message.trimmed();
    if (!message.isEmpty() && message != "OK" && message != "Demo data") {
        QMessageBox::information(this, "操作成功", message);
    }
    if (responseAction != "list" && responseAction != "waiting") {
        refresh();
    }
}

void ModulePage::requestCascadeFilterDoctors()
{
    if (!usesDoctorCascadeFilter() || !m_apiClient) {
        return;
    }

    common::Request request;
    request.module = "doctor";
    request.action = "list";
    m_apiClient->send(request);
}

void ModulePage::fillTable(const QJsonArray& rows)
{
    m_rows = rows;
    rebuildGroupFilter();
    m_currentPage = 0;
    renderPage();
    rowsUpdated(m_rows);
}

void ModulePage::renderPage()
{
    m_table->clear();
    const QJsonArray rows = filteredRows();

    if (rows.isEmpty()) {
        m_table->setRowCount(0);
        m_table->setColumnCount(1);
        m_table->setHorizontalHeaderLabels({"暂无数据"});
        resizeColumnsForReading(m_table);
        updatePageLabel();
        return;
    }

    const auto firstRow = rows.first().toObject();
    const QStringList headers = preferredHeaders(firstRow);
    const int start = m_currentPage * m_pageSize;
    const int end = qMin(start + m_pageSize, rows.size());

    m_table->setColumnCount(headers.size());
    m_table->setRowCount(end - start);
    m_table->setHorizontalHeaderLabels(headers);

    for (int rowIndex = start; rowIndex < end; ++rowIndex) {
        const auto row = rows.at(rowIndex).toObject();
        const int tableRow = rowIndex - start;
        m_table->setRowHeight(tableRow, 40);
        for (int columnIndex = 0; columnIndex < headers.size(); ++columnIndex) {
            const QString key = headers.at(columnIndex);
            const QVariant rawValue = row.value(key).toVariant();
            const QString value = displayValueForCell(m_module, key, row);
            auto* item = new QTableWidgetItem(value);
            item->setToolTip(value);
            item->setData(Qt::UserRole, rawValue);
            applyWorkstationCellStyle(item, key, value, row);
            m_table->setItem(tableRow, columnIndex, item);
        }
    }

    resizeColumnsForReading(m_table);
    updatePageLabel();
}

void ModulePage::updatePageLabel()
{
    const int total = filteredRows().size();
    const int pageCount = pageCountForRows(total);
    const QString group = effectiveGroup();
    if (group.isEmpty()) {
        m_pageLabel->setText(QString("第 %1/%2 页，共 %3 条")
            .arg(total == 0 ? 0 : m_currentPage + 1)
            .arg(pageCount)
            .arg(total));
    } else {
        QString prefix = group;
        if (usesDoctorCascadeFilter() && m_selectedDoctorFilter != "全部") {
            prefix += " / " + m_selectedDoctorFilter;
        }
        if (usesDoctorCascadeFilter() && m_selectedClinicTypeFilter != "全部") {
            prefix += " / " + m_selectedClinicTypeFilter;
        }
        m_pageLabel->setText(QString("%1：第 %2/%3 页，共 %4 条")
            .arg(prefix)
            .arg(total == 0 ? 0 : m_currentPage + 1)
            .arg(pageCount)
            .arg(total));
    }

    m_prevButton->setEnabled(m_currentPage > 0);
    m_nextButton->setEnabled(m_currentPage + 1 < pageCount);
}

void ModulePage::previousPage()
{
    if (m_currentPage > 0) {
        --m_currentPage;
    } else {
        return;
    }
    renderPage();
}

void ModulePage::nextPage()
{
    const int total = filteredRows().size();
    const int pageCount = pageCountForRows(total);
    if (m_currentPage + 1 < pageCount) {
        ++m_currentPage;
    } else {
        return;
    }
    renderPage();
}

void ModulePage::changePageSize(int index)
{
    const QList<int> sizes = {10, 15, 30};
    if (index >= 0 && index < sizes.size()) {
        m_pageSize = sizes.at(index);
    }
    m_currentPage = 0;
    renderPage();
}

void ModulePage::changeGroupFilter(int)
{
    m_selectedGroup = m_groupBox->currentText();
    if (usesDoctorCascadeFilter()) {
        rebuildGroupFilter();
    }
    m_currentPage = 0;
    renderPage();
    if (usesDoctorCascadeFilter()) {
        refresh();
    }
}

void ModulePage::changeDoctorFilter(int)
{
    m_selectedDoctorFilter = m_doctorFilterBox ? m_doctorFilterBox->currentText() : QString("全部");
    m_selectedClinicTypeFilter = "全部";
    if (m_clinicTypeFilterBox) {
        QSignalBlocker blocker(m_clinicTypeFilterBox);
        m_clinicTypeFilterBox->setCurrentIndex(0);
    }
    m_currentPage = 0;
    renderPage();
    if (usesDoctorCascadeFilter()) {
        refresh();
    }
}

void ModulePage::changeClinicTypeFilter(int)
{
    m_selectedClinicTypeFilter = m_clinicTypeFilterBox ? m_clinicTypeFilterBox->currentText() : QString("全部");
    m_currentPage = 0;
    renderPage();
    if (usesDoctorCascadeFilter()) {
        refresh();
    }
}

void ModulePage::changeDateFilter()
{
    if (m_dateFilterBox && m_dateEdit) {
        m_dateEdit->setVisible(m_dateFilterBox->currentText() == "指定日期");
    }
    rebuildGroupFilter();
    m_currentPage = 0;
    renderPage();
    refresh();
}

void ModulePage::applySearch()
{
    setSearchKeyword(m_searchEdit->text());
    m_currentPage = 0;
    renderPage();
}

void ModulePage::clearSearch()
{
    setSearchKeyword({});
    m_currentPage = 0;
    renderPage();
}

void ModulePage::exportCsv()
{
    const QJsonArray rows = filteredRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, "暂无数据", "当前没有可导出的数据。");
        return;
    }

    const QStringList headers = preferredHeaders(rows.first().toObject());
    const QString defaultName = exportTitle() + "_" + QDate::currentDate().toString("yyyyMMdd") + ".csv";
    const QString path = QFileDialog::getSaveFileName(this, "导出" + exportTitle(), defaultName, "CSV 文件 (*.csv)");
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法写入文件：" + path);
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QChar(0xFEFF);
    for (int i = 0; i < headers.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << csvEscape(headers.at(i));
    }
    out << '\n';

    for (const auto& item : rows) {
        const auto row = item.toObject();
        for (int i = 0; i < headers.size(); ++i) {
            if (i > 0) {
                out << ',';
            }
            out << csvEscape(row.value(headers.at(i)).toVariant().toString());
        }
        out << '\n';
    }

    QMessageBox::information(this, "导出成功", QString("已导出 %1 条数据。").arg(rows.size()));
}

void ModulePage::editSelectedRow()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择数据", "请先在表格中点击要修改的一行。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("修改当前数据");
    auto* form = new QFormLayout(&dialog);
    QMap<QString, QLineEdit*> edits;
    const QStringList readonlyKeys = {"患者编号", "挂号单号", "账单号", "药品编码", "处方号", "接诊时间", "创建时间", "挂号时间"};

    for (auto it = row.constBegin(); it != row.constEnd(); ++it) {
        auto* edit = new QLineEdit(it.value().toVariant().toString(), &dialog);
        edit->setReadOnly(readonlyKeys.contains(it.key()));
        form->addRow(it.key(), edit);
        edits.insert(it.key(), edit);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QJsonObject payload;
    for (auto it = edits.constBegin(); it != edits.constEnd(); ++it) {
        payload[it.key()] = it.value()->text().trimmed();
    }

    common::Request request;
    request.module = m_module;
    request.action = "update";
    request.payload = payload;
    m_apiClient->send(request);
}

void ModulePage::deleteSelectedRow()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择数据", "请先在表格中点击要删除或停用的一行。");
        return;
    }

    if (QMessageBox::question(this, "确认操作", "确定要删除/停用当前选中数据吗？") != QMessageBox::Yes) {
        return;
    }

    common::Request request;
    request.module = m_module;
    request.action = "delete";
    request.payload = row;
    m_apiClient->send(request);
}

void ModulePage::showTableContextMenu(const QPoint& position)
{
    if (!m_table || m_table->currentRow() < 0) {
        return;
    }

    QMenu menu(this);
    auto* copyCellAction = menu.addAction("复制当前单元格");
    connect(copyCellAction, &QAction::triggered, this, &ModulePage::copyCurrentCell);

    const QString registrationNo = selectedFieldText("挂号单号");
    if (!registrationNo.isEmpty()) {
        auto* copyRegistrationAction = menu.addAction("复制挂号单号");
        connect(copyRegistrationAction, &QAction::triggered, this, &ModulePage::copySelectedRegistrationNo);
    }

    menu.exec(m_table->viewport()->mapToGlobal(position));
}

void ModulePage::copyCurrentCell()
{
    const QString text = currentCellText();
    if (text.isEmpty()) {
        return;
    }
    QApplication::clipboard()->setText(text);
}

void ModulePage::copySelectedRegistrationNo()
{
    const QString registrationNo = selectedFieldText("挂号单号");
    if (registrationNo.isEmpty()) {
        return;
    }
    QApplication::clipboard()->setText(registrationNo);
}

void ModulePage::rebuildGroupFilter()
{
    QSet<QString> groups;
    const QJsonArray optionRows = cascadeFilterRows();
    for (const auto& item : optionRows) {
        const QString group = groupValueOf(item.toObject());
        if (!group.isEmpty()) {
            groups.insert(group);
        }
    }

    const QString previous = m_selectedGroup.isEmpty() ? QString("全部") : m_selectedGroup;
    QSignalBlocker blocker(m_groupBox);
    m_groupBox->clear();
    m_groupBox->addItem("全部");

    QStringList sortedGroups = groups.values();
    sortedGroups.sort(Qt::CaseInsensitive);
    m_groupPages = sortedGroups;
    m_groupBox->addItems(sortedGroups);

    const int index = m_groupBox->findText(previous);
    m_groupBox->setCurrentIndex(index >= 0 ? index : 0);
    m_selectedGroup = m_groupBox->currentText();
    m_groupBox->setEnabled(true);

    if (m_doctorFilterBox) {
        QSet<QString> doctors;
        for (const auto& item : optionRows) {
            const auto row = item.toObject();
            const QString group = effectiveGroup();
            if (!group.isEmpty() && groupValueOf(row) != group) {
                continue;
            }
            const QString doctor = doctorValueOf(row);
            if (!doctor.isEmpty()) {
                doctors.insert(doctor);
            }
        }

        const QString previousDoctor = m_selectedDoctorFilter.isEmpty() ? QString("全部") : m_selectedDoctorFilter;
        const QString loggedInDoctor = loggedInDoctorFilterName();
        QSignalBlocker doctorBlocker(m_doctorFilterBox);
        m_doctorFilterBox->clear();
        if (usesDoctorSelfScope()) {
            m_doctorFilterBox->addItem(loggedInDoctor.isEmpty() ? QStringLiteral("当前医生") : loggedInDoctor);
            m_doctorFilterBox->setCurrentIndex(0);
        } else {
            m_doctorFilterBox->addItem("全部");
            QStringList sortedDoctors = doctors.values();
            sortedDoctors.sort(Qt::CaseInsensitive);
            m_doctorFilterBox->addItems(sortedDoctors);
            const int doctorIndex = m_doctorFilterBox->findText(previousDoctor);
            m_doctorFilterBox->setCurrentIndex(doctorIndex >= 0 ? doctorIndex : 0);
        }
        m_selectedDoctorFilter = m_doctorFilterBox->currentText();
        if (usesDoctorSelfScope()) {
            m_doctorFilterBox->setEnabled(false);
        } else {
            m_doctorFilterBox->setEnabled(true);
        }

        if (m_clinicTypeFilterBox) {
            QSet<QString> clinicTypes;
            for (const auto& item : optionRows) {
                const auto row = item.toObject();
                const QString group = effectiveGroup();
                if (!group.isEmpty() && groupValueOf(row) != group) {
                    continue;
                }
                const QString doctor = doctorValueOf(row);
                if (m_selectedDoctorFilter != "全部" && doctor != m_selectedDoctorFilter) {
                    continue;
                }
                const QString clinicType = clinicTypeValueOf(row);
                if (!clinicType.isEmpty()) {
                    clinicTypes.insert(clinicType);
                }
            }

            const QString previousClinicType = m_selectedClinicTypeFilter.isEmpty() ? QString("全部") : m_selectedClinicTypeFilter;
            QSignalBlocker clinicTypeBlocker(m_clinicTypeFilterBox);
            m_clinicTypeFilterBox->clear();
            m_clinicTypeFilterBox->addItem("全部");
            QStringList sortedClinicTypes = clinicTypes.values();
            sortedClinicTypes.sort(Qt::CaseInsensitive);
            m_clinicTypeFilterBox->addItems(sortedClinicTypes);
            const int clinicTypeIndex = m_clinicTypeFilterBox->findText(previousClinicType);
            m_clinicTypeFilterBox->setCurrentIndex(clinicTypeIndex >= 0 ? clinicTypeIndex : 0);
            m_selectedClinicTypeFilter = m_clinicTypeFilterBox->currentText();
            m_clinicTypeFilterBox->setEnabled(true);
        }
    }
}

QJsonArray ModulePage::filteredRows() const
{
    const QJsonArray source = searchedRows();
    const QString group = effectiveGroup();
    const bool hasDoctorFilter = usesDoctorCascadeFilter() && m_selectedDoctorFilter != "全部";
    const bool hasClinicTypeFilter = usesDoctorCascadeFilter() && m_selectedClinicTypeFilter != "全部";
    if (group.isEmpty() && !hasDoctorFilter && !hasClinicTypeFilter) {
        return source;
    }

    QJsonArray rows;
    for (const auto& item : source) {
        const auto row = item.toObject();
        if (!group.isEmpty() && groupValueOf(row) != group) {
            continue;
        }
        if (doctorValueOf(row) != m_selectedDoctorFilter
            && hasDoctorFilter) {
            continue;
        }
        if (clinicTypeValueOf(row) != m_selectedClinicTypeFilter
            && hasClinicTypeFilter) {
            continue;
        }
        rows.append(row);
    }
    return rows;
}

QJsonArray ModulePage::cascadeFilterRows() const
{
    if (!usesDoctorCascadeFilter() || m_cascadeFilterDoctors.isEmpty()) {
        return QJsonArray();
    }

    QJsonArray rows;
    for (const auto& item : m_cascadeFilterDoctors) {
        const auto doctor = item.toObject();
        const QString name = doctor.value("医生姓名").toString().trimmed();
        const QString department = doctor.value("所属科室").toString().trimmed();
        if (name.isEmpty() || department.isEmpty()) {
            continue;
        }

        const QString title = doctor.value("职称").toString().trimmed();
        const double fee = doctor.value("挂号费").toVariant().toDouble();
        QJsonObject row;
        row["科室"] = department;
        row["医生"] = name;
        row["职称"] = title;
        row["号别"] = (title.contains("主任") || fee >= 30.0) ? QString("专家号") : QString("普通号");
        rows.append(row);
    }
    return rows;
}

QJsonArray ModulePage::searchedRows() const
{
    QJsonArray rows;
    for (const auto& item : m_rows) {
        const auto row = item.toObject();
        if (!matchesDateFilter(row)) {
            continue;
        }
        if (m_keyword.isEmpty()) {
            rows.append(row);
            continue;
        }
        bool matched = false;
        for (auto it = row.constBegin(); it != row.constEnd(); ++it) {
            const QString rawValue = it.value().toVariant().toString();
            const QString displayValue = displayValueForCell(m_module, it.key(), row);
            if (rawValue.contains(m_keyword, Qt::CaseInsensitive)
                || displayValue.contains(m_keyword, Qt::CaseInsensitive)) {
                matched = true;
                break;
            }
        }
        if (matched) {
            rows.append(row);
        }
    }
    return rows;
}

bool ModulePage::matchesDateFilter(const QJsonObject& row) const
{
    if (!m_dateFilterBox || !m_dateEdit || m_dateFilterBox->currentText() == "全部日期") {
        return true;
    }

    const QDate rowDate = dateValueOf(row);
    if (!rowDate.isValid()) {
        return false;
    }

    const QString filter = m_dateFilterBox->currentText();
    const QDate today = QDate::currentDate();
    if (filter == "今天") {
        return rowDate == today;
    }
    if (filter == "本周") {
        const QDate weekStart = today.addDays(1 - today.dayOfWeek());
        return rowDate >= weekStart && rowDate <= weekStart.addDays(6);
    }
    if (filter == "本月") {
        return rowDate.year() == today.year() && rowDate.month() == today.month();
    }
    if (filter == "指定日期") {
        return rowDate == m_dateEdit->date();
    }
    return true;
}

QString ModulePage::groupValueOf(const QJsonObject& row) const
{
    if (m_module == "consultation" || (m_module == "registration" && m_action == "waiting")) {
        return row.value("科室").toString().trimmed();
    }

    for (const QString& key : groupKeys()) {
        const QString value = displayValueForCell(m_module, key, row).trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QString ModulePage::doctorValueOf(const QJsonObject& row) const
{
    return row.value("医生").toString().trimmed();
}

QString ModulePage::clinicTypeValueOf(const QJsonObject& row) const
{
    return row.value("号别").toString().trimmed();
}

QDate ModulePage::dateValueOf(const QJsonObject& row) const
{
    const QStringList dateKeys = {"就诊日期", "挂号时间", "接诊时间", "开方时间", "创建时间"};
    for (const QString& key : dateKeys) {
        const QString value = row.value(key).toVariant().toString().trimmed();
        if (value.isEmpty()) {
            continue;
        }
        const QDate date = QDate::fromString(value.left(10), "yyyy-MM-dd");
        if (date.isValid()) {
            return date;
        }
    }
    return {};
}

QString ModulePage::effectiveGroup() const
{
    if (m_selectedGroup == "全部") {
        return {};
    }

    return m_selectedGroup;
}

bool ModulePage::usesDoctorSelfScope() const
{
    return usesDoctorCascadeFilter()
        && m_apiClient
        && m_apiClient->roleCode().compare("DOCTOR", Qt::CaseInsensitive) == 0;
}

QString ModulePage::loggedInDoctorFilterName() const
{
    if (!m_apiClient) {
        return {};
    }

    const QString realName = m_apiClient->realName().trimmed();
    if (!realName.isEmpty()) {
        return realName;
    }
    return m_apiClient->username().trimmed();
}

bool ModulePage::usesDoctorCascadeFilter() const
{
    return m_module == "consultation" || (m_module == "registration" && m_action == "waiting");
}

bool ModulePage::usesDateFilter() const
{
    return m_module == "registration" || m_module == "consultation" || m_module == "patientRecord";
}

bool ModulePage::defaultsToTodayDateFilter() const
{
    return m_module == "consultation" || (m_module == "registration" && m_action == "waiting");
}

QString ModulePage::groupTitle() const
{
    if (m_module == "registration" && m_action == "waiting") {
        return "科室";
    }
    if (m_module == "registration" || m_module == "billing" || m_module == "prescription") {
        return "状态";
    }
    if (m_module == "schedule" || m_module == "statistics") {
        return "日期";
    }
    if (m_module == "inventory") {
        return "分类";
    }
    if (m_module == "examination") {
        return "状态";
    }
    if (m_module == "doctor") {
        return "科室";
    }
    if (m_module == "patient") {
        return "患者状态";
    }
    if (m_module == "consultation") {
        return "科室";
    }
    return "分组";
}

QStringList ModulePage::groupKeys() const
{
    if (m_module == "registration" && m_action == "waiting") {
        return {"科室"};
    }
    if (m_module == "registration" || m_module == "billing" || m_module == "prescription") {
        return {"状态"};
    }
    if (m_module == "schedule") {
        return {"出诊日期"};
    }
    if (m_module == "statistics") {
        return {"统计日期"};
    }
    if (m_module == "inventory") {
        return {"分类"};
    }
    if (m_module == "examination") {
        return {"状态"};
    }
    if (m_module == "doctor") {
        return {"所属科室", "科室"};
    }
    if (m_module == "patient") {
        return {"患者状态", "身份登记"};
    }
    if (m_module == "consultation") {
        return {"科室"};
    }
    return {"科室", "所属科室", "状态", "分类"};
}

QStringList ModulePage::preferredHeaders(const QJsonObject& row) const
{
    QStringList preferred;
    if (m_module == "patient") {
        preferred = {"患者编号", "姓名", "性别", "电话", "身份证号", "身份登记", "患者状态", "就诊次数", "最近就诊", "档案完整度", "智能提示", "地址", "建档时间"};
    } else if (m_module == "department") {
        preferred = {"科室编码", "科室名称", "位置", "状态"};
    } else if (m_module == "doctor") {
        preferred = {"医生姓名", "所属科室", "职称", "擅长方向", "挂号费", "电话", "状态"};
    } else if (m_module == "schedule") {
        preferred = {"科室", "医生", "职称", "出诊日期", "总号源", "剩余号源", "状态"};
    } else if (m_module == "registration" && m_action == "waiting") {
        preferred = {"急诊标识", "急诊原因", "科室", "医生", "职称", "号别", "患者", "身份证号", "挂号单号", "就诊日期", "时段", "候诊状态", "排队序号", "预计等待", "挂号时间"};
    } else if (m_module == "registration") {
        preferred = {"挂号单号", "患者", "身份证号", "科室", "医生", "就诊日期", "时段", "状态", "挂号费", "挂号时间"};
    } else if (m_module == "consultation") {
        preferred = {"急诊标识", "急诊原因", "科室", "医生", "职称", "号别", "患者", "身份证号", "挂号单号", "就诊日期", "时段", "状态", "主诉", "诊断", "医嘱", "接诊时间"};
    } else if (m_module == "examination") {
        preferred = {"检查单号", "挂号单号", "患者", "身份证号", "医生", "检查项目", "申请说明", "检查结果", "状态", "申请时间", "完成时间"};
    } else if (m_module == "prescription") {
        preferred = {"处方号", "挂号单号", "患者", "身份证号", "医生", "状态", "药品明细", "处方金额",
                     "驳回原因", "退药原因", "审核时间", "发药时间", "退药时间", "开方时间"};
    } else if (m_module == "inventory") {
        preferred = {"药品编码", "条形码", "药品名称", "分类", "规格", "单位", "售价", "库存", "预警库存", "有效期", "预警原因", "状态"};
    } else if (m_module == "patientRecord") {
        preferred = {"患者编号", "患者", "身份证号", "电话", "挂号单号", "科室", "医生", "就诊日期", "时段", "挂号状态", "主诉", "现病史", "既往史", "体格检查", "ICD编码", "诊断", "医嘱", "外院报告医院", "外院报告类型", "外院报告日期", "外院报告结论", "外院报告附件", "接诊时间", "处方号", "处方状态", "处方金额", "账单号", "费用合计", "账单状态"};
    } else if (m_module == "operationLog") {
        preferred = {"操作人", "模块", "动作", "内容", "操作时间"};
    }

    QStringList headers;
    for (const QString& key : preferred) {
        if (row.contains(key)) {
            headers.append(key);
        }
    }
    for (auto it = row.constBegin(); it != row.constEnd(); ++it) {
        if (!headers.contains(it.key())) {
            headers.append(it.key());
        }
    }
    return headers;
}

QJsonObject ModulePage::selectedRowObject() const
{
    const int rowIndex = m_table->currentRow();
    if (rowIndex < 0 || m_table->columnCount() <= 0) {
        return {};
    }

    QJsonObject object;
    for (int column = 0; column < m_table->columnCount(); ++column) {
        const auto* header = m_table->horizontalHeaderItem(column);
        const auto* item = m_table->item(rowIndex, column);
        if (!header || !item) {
            continue;
        }
        const QVariant rawValue = item->data(Qt::UserRole);
        object[header->text()] = QJsonValue::fromVariant(rawValue.isValid() ? rawValue : QVariant(item->text()));
    }
    return object;
}

QString ModulePage::currentCellText() const
{
    if (!m_table || m_table->currentRow() < 0 || m_table->currentColumn() < 0) {
        return {};
    }

    const auto* item = m_table->item(m_table->currentRow(), m_table->currentColumn());
    if (!item) {
        return {};
    }

    const QVariant rawValue = item->data(Qt::UserRole);
    return rawValue.isValid() ? rawValue.toString().trimmed() : item->text().trimmed();
}

QString ModulePage::selectedFieldText(const QString& fieldName) const
{
    const QJsonObject row = selectedRowObject();
    return row.value(fieldName).toVariant().toString().trimmed();
}

bool ModulePage::supportsCrud() const
{
    return supportsEdit() || supportsDelete();
}

bool ModulePage::supportsEdit() const
{
    if (m_module == "registration" && m_action == "waiting") {
        return false;
    }

    return m_module == "patient"
        || m_module == "schedule"
        || m_module == "doctor"
        || m_module == "inventory"
        || m_module == "patientRecord";
}

bool ModulePage::supportsDelete() const
{
    if (m_module == "registration" && m_action == "waiting") {
        return false;
    }

    return m_module == "patient"
        || m_module == "registration"
        || m_module == "schedule"
        || m_module == "doctor"
        || m_module == "inventory"
        || m_module == "patientRecord";
}

bool ModulePage::supportsExport() const
{
    return (m_module == "patient" && m_action == "list")
        || (m_module == "registration" && m_action == "list")
        || (m_module == "patientRecord" && m_action == "list")
        || (m_module == "operationLog" && m_action == "list");
}

QString ModulePage::exportTitle() const
{
    if (m_module == "patient") {
        return "患者管理";
    }
    if (m_module == "registration") {
        return "挂号管理";
    }
    if (m_module == "patientRecord") {
        return "患者病历档案";
    }
    if (m_module == "operationLog") {
        return "操作日志";
    }
    return "数据";
}

int ModulePage::pageCountForRows(int rowCount) const
{
    return rowCount == 0 ? 0 : (rowCount + m_pageSize - 1) / m_pageSize;
}

} // namespace hospital::client

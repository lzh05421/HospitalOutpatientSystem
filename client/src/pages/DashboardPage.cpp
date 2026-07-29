#include "client/pages/Pages.h"

#include "client/ApiClient.h"

#include <QAbstractItemView>
#include <QDate>
#include <QDateEdit>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTextStream>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace hospital::client {
namespace {

class DashboardChartWidget : public QWidget
{
public:
    enum class Mode {
        Line,
        Bar
    };

    DashboardChartWidget(QString title, QString labelKey, QString valueKey, Mode mode, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_title(std::move(title))
        , m_labelKey(std::move(labelKey))
        , m_valueKey(std::move(valueKey))
        , m_mode(mode)
    {
        setMinimumHeight(220);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    void setRows(const QJsonArray& rows)
    {
        m_rows = rows;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor("#ffffff"));

        const QRectF bounds = rect().adjusted(16, 14, -16, -14);
        painter.setPen(QPen(QColor("#172033")));
        QFont titleFont = painter.font();
        titleFont.setBold(true);
        titleFont.setPointSize(11);
        painter.setFont(titleFont);
        painter.drawText(bounds.adjusted(0, 0, 0, -bounds.height() + 24), Qt::AlignLeft | Qt::AlignVCenter, m_title);

        QRectF plot = bounds.adjusted(8, 38, -8, -34);
        painter.setPen(QPen(QColor("#d8dee8")));
        painter.drawRoundedRect(plot, 6, 6);

        if (m_rows.isEmpty()) {
            painter.setPen(QColor("#64748b"));
            painter.drawText(plot, Qt::AlignCenter, "暂无统计数据");
            return;
        }

        QVector<QString> labels;
        QVector<double> values;
        double maxValue = 0;
        for (const auto& item : m_rows) {
            const auto row = item.toObject();
            labels.append(row.value(m_labelKey).toVariant().toString());
            const double value = row.value(m_valueKey).toVariant().toDouble();
            values.append(value);
            maxValue = std::max(maxValue, value);
        }
        maxValue = std::max(1.0, maxValue);

        painter.setPen(QPen(QColor("#eef2f7")));
        for (int i = 1; i < 4; ++i) {
            const double y = plot.top() + plot.height() * i / 4.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }

        if (m_mode == Mode::Line) {
            drawPolyline(&painter, plot, labels, values, maxValue);
        } else {
            drawBars(&painter, plot, labels, values, maxValue);
        }
    }

private:
    void drawPolyline(QPainter* painter, const QRectF& plot, const QVector<QString>& labels, const QVector<double>& values, double maxValue)
    {
        if (values.isEmpty()) {
            return;
        }

        QVector<QPointF> points;
        const int denominator = std::max(1, static_cast<int>(values.size()) - 1);
        for (int i = 0; i < values.size(); ++i) {
            const double x = plot.left() + plot.width() * i / denominator;
            const double y = plot.bottom() - plot.height() * values.at(i) / maxValue;
            points.append(QPointF(x, y));
        }

        painter->setPen(QPen(QColor("#2563eb"), 2.4));
        painter->drawPolyline(points.constData(), points.size());
        painter->setBrush(QColor("#2563eb"));
        for (int i = 0; i < points.size(); ++i) {
            painter->drawEllipse(points.at(i), 4, 4);
            painter->setPen(QColor("#334155"));
            painter->drawText(QRectF(points.at(i).x() - 28, points.at(i).y() - 24, 56, 18),
                              Qt::AlignCenter, QString::number(values.at(i), 'f', 0));
            painter->setPen(QPen(QColor("#2563eb"), 2.4));
        }
        drawLabels(painter, plot, labels);
    }

    void drawBars(QPainter* painter, const QRectF& plot, const QVector<QString>& labels, const QVector<double>& values, double maxValue)
    {
        const int count = values.size();
        if (count == 0) {
            return;
        }

        const double slotWidth = plot.width() / count;
        const double barWidth = std::max(18.0, slotWidth * 0.52);
        painter->setBrush(QColor("#14b8a6"));
        painter->setPen(Qt::NoPen);
        for (int i = 0; i < count; ++i) {
            const double barHeight = plot.height() * values.at(i) / maxValue;
            const double x = plot.left() + slotWidth * i + (slotWidth - barWidth) / 2.0;
            const QRectF bar(x, plot.bottom() - barHeight, barWidth, barHeight);
            painter->drawRect(bar);
            painter->setPen(QColor("#334155"));
            painter->drawText(QRectF(x - 10, bar.top() - 22, barWidth + 20, 18),
                              Qt::AlignCenter, QString::number(values.at(i), 'f', 0));
            painter->setPen(Qt::NoPen);
        }
        drawLabels(painter, plot, labels);
    }

    void drawLabels(QPainter* painter, const QRectF& plot, const QVector<QString>& labels)
    {
        painter->setPen(QColor("#64748b"));
        QFont labelFont = painter->font();
        labelFont.setPointSize(8);
        painter->setFont(labelFont);
        const int count = labels.size();
        const int step = count > 8 ? 2 : 1;
        for (int i = 0; i < count; i += step) {
            const double x = count == 1
                ? plot.center().x()
                : plot.left() + plot.width() * i / std::max(1, count - 1);
            QString label = labels.at(i);
            if (label.size() > 8) {
                label = label.left(8);
            }
            painter->drawText(QRectF(x - 44, plot.bottom() + 8, 88, 20), Qt::AlignCenter, label);
        }
    }

    QString m_title;
    QString m_labelKey;
    QString m_valueKey;
    Mode m_mode;
    QJsonArray m_rows;
};

QLabel* metricValue(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    QFont font = label->font();
    font.setPointSize(22);
    font.setBold(true);
    label->setFont(font);
    label->setStyleSheet("color:#12324a;");
    return label;
}

QFrame* metricCard(const QString& title, QLabel* value, QWidget* parent)
{
    auto* box = new QFrame(parent);
    box->setObjectName("dashboardMetricCard");
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(6);
    auto* titleLabel = new QLabel(title, box);
    titleLabel->setObjectName("dashboardMetricTitle");
    layout->addWidget(titleLabel);
    layout->addWidget(value);
    layout->addStretch();
    return box;
}

QFrame* sectionCard(const QString& title, QWidget* content, QWidget* parent)
{
    auto* box = new QFrame(parent);
    box->setObjectName("dashboardSectionCard");
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    auto* titleLabel = new QLabel(title, box);
    titleLabel->setObjectName("dashboardSectionTitle");
    layout->addWidget(titleLabel);
    if (content) {
        content->setObjectName("chartSurface");
        layout->addWidget(content);
    }
    return box;
}

void fillTable(QTableWidget* table, const QJsonArray& rows, const QStringList& headers)
{
    table->clear();
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setRowCount(rows.size());
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setShowGrid(false);
    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const auto row = rows.at(rowIndex).toObject();
        for (int column = 0; column < headers.size(); ++column) {
            const QString value = row.value(headers.at(column)).toVariant().toString();
            auto* item = new QTableWidgetItem(value);
            item->setToolTip(value);
            table->setItem(rowIndex, column, item);
        }
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setStretchLastSection(true);
    table->resizeColumnsToContents();
}

DashboardChartWidget* chart(QWidget* parent, const QString& title, const QString& labelKey, const QString& valueKey, DashboardChartWidget::Mode mode)
{
    auto* widget = new DashboardChartWidget(title, labelKey, valueKey, mode, parent);
    widget->setObjectName(title);
    return widget;
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

} // namespace

DashboardPage::DashboardPage(ApiClient* apiClient, QWidget* parent)
    : QWidget(parent)
    , m_apiClient(apiClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(14);

    auto* dashboardHero = new QFrame(this);
    dashboardHero->setObjectName("dashboardHero");
    auto* heroLayout = new QHBoxLayout(dashboardHero);
    heroLayout->setContentsMargins(18, 18, 18, 18);
    heroLayout->setSpacing(16);

    auto* heroTextLayout = new QVBoxLayout();
    heroTextLayout->setSpacing(6);
    auto* header = new QHBoxLayout();
    auto* title = new QLabel("院长驾驶舱", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto* trendSummary = new QLabel("今日门诊态势", dashboardHero);
    trendSummary->setObjectName("trendSummary");
    auto* heroHint = new QLabel("聚合门诊量、候诊压力、接诊效率、收入和库存预警，快速了解当日院内运行状态。", dashboardHero);
    heroHint->setObjectName("heroHint");
    heroHint->setWordWrap(true);
    heroTextLayout->addWidget(trendSummary);
    heroTextLayout->addWidget(title);
    heroTextLayout->addWidget(heroHint);

    m_startDateEdit = new QDateEdit(QDate::currentDate().addDays(-6), this);
    m_endDateEdit = new QDateEdit(QDate::currentDate(), this);
    for (auto* edit : {m_startDateEdit, m_endDateEdit}) {
        edit->setCalendarPopup(true);
        edit->setDisplayFormat("yyyy-MM-dd");
    }
    auto* refreshButton = new QPushButton("查询", this);
    refreshButton->setObjectName("primaryButton");
    auto* exportButton = new QPushButton("导出CSV", this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(new QLabel("开始", this));
    header->addWidget(m_startDateEdit);
    header->addWidget(new QLabel("结束", this));
    header->addWidget(m_endDateEdit);
    header->addWidget(refreshButton);
    header->addWidget(exportButton);
    heroLayout->addLayout(heroTextLayout, 1);
    heroLayout->addLayout(header);
    root->addWidget(dashboardHero);

    auto* grid = new QGridLayout();
    m_todayRegistrations = metricValue("0", this);
    m_waitingPatients = metricValue("0", this);
    m_finishedPatients = metricValue("0", this);
    m_todayIncome = metricValue("0.00", this);
    m_incomeMix = metricValue("挂号 0.00 / 药品 0.00", this);
    grid->addWidget(metricCard("今日挂号量", m_todayRegistrations, this), 0, 0);
    grid->addWidget(metricCard("当前候诊人数", m_waitingPatients, this), 0, 1);
    grid->addWidget(metricCard("今日已接诊", m_finishedPatients, this), 0, 2);
    grid->addWidget(metricCard("今日收入", m_todayIncome, this), 1, 0);
    grid->addWidget(metricCard("收入构成", m_incomeMix, this), 1, 1, 1, 2);
    root->addLayout(grid);

    auto* charts = new QGridLayout();
    m_dailyVisitsChart = chart(this, "日门诊量趋势", "统计日期", "门诊量", DashboardChartWidget::Mode::Line);
    m_departmentVisitsChart = chart(this, "各科室接诊量", "科室", "接诊量", DashboardChartWidget::Mode::Bar);
    m_doctorRankingChart = chart(this, "医生接诊排行", "医生", "接诊量", DashboardChartWidget::Mode::Bar);
    charts->addWidget(sectionCard("日门诊量趋势", m_dailyVisitsChart, this), 0, 0, 1, 2);
    charts->addWidget(sectionCard("各科室接诊量", m_departmentVisitsChart, this), 1, 0);
    charts->addWidget(sectionCard("医生接诊排行", m_doctorRankingChart, this), 1, 1);
    root->addLayout(charts, 2);

    auto* tables = new QHBoxLayout();
    auto* doctorBox = new QFrame(this);
    doctorBox->setObjectName("dashboardSectionCard");
    auto* doctorLayout = new QVBoxLayout(doctorBox);
    doctorLayout->setContentsMargins(16, 14, 16, 14);
    doctorLayout->setSpacing(10);
    auto* doctorTitle = new QLabel("今日接诊 Top5", doctorBox);
    doctorTitle->setObjectName("dashboardSectionTitle");
    m_doctorTable = new QTableWidget(doctorBox);
    m_doctorTable->setObjectName("chartSurface");
    doctorLayout->addWidget(doctorTitle);
    doctorLayout->addWidget(m_doctorTable);
    auto* warningBox = new QFrame(this);
    warningBox->setObjectName("dashboardSectionCard");
    auto* warningLayout = new QVBoxLayout(warningBox);
    warningLayout->setContentsMargins(16, 14, 16, 14);
    warningLayout->setSpacing(10);
    auto* warningTitle = new QLabel("库存预警", warningBox);
    warningTitle->setObjectName("dashboardSectionTitle");
    m_warningTable = new QTableWidget(warningBox);
    m_warningTable->setObjectName("chartSurface");
    warningLayout->addWidget(warningTitle);
    warningLayout->addWidget(m_warningTable);
    tables->addWidget(doctorBox);
    tables->addWidget(warningBox);
    root->addLayout(tables, 1);

    setStyleSheet(R"(
        QLabel { color: #18212f; }
        QFrame#dashboardHero {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8fbfb, stop:1 #ecf7f5);
            border: 1px solid #d4e5e1;
            border-radius: 16px;
        }
        QLabel#trendSummary {
            min-width: 96px;
            max-width: 120px;
            padding: 6px 14px;
            border-radius: 999px;
            background: #dff7f3;
            color: #0f766e;
            border: 1px solid #99f6e4;
            font-size: 12px;
            font-weight: 700;
        }
        QLabel#heroHint {
            color: #5f7080;
            font-size: 13px;
        }
        QFrame#dashboardMetricCard, QFrame#dashboardSectionCard {
            background: #ffffff;
            border: 1px solid #d7e4e2;
            border-radius: 14px;
        }
        QLabel#dashboardMetricTitle {
            color: #5f7080;
            font-size: 12px;
            font-weight: 600;
        }
        QLabel#dashboardSectionTitle {
            color: #0f172a;
            font-size: 14px;
            font-weight: 700;
        }
        QWidget#chartSurface {
            background: transparent;
        }
        QTableWidget {
            background: #ffffff;
            alternate-background-color: #f8fafc;
            border: 1px solid #dce6e6;
            border-radius: 10px;
            gridline-color: #eef2f7;
            selection-background-color: #dff7f3;
        }
        QHeaderView::section {
            background: #edf7f5;
            color: #334155;
            border: none;
            border-right: 1px solid #dce6e6;
            border-bottom: 1px solid #dce6e6;
            padding: 8px;
            font-weight: 600;
        }
        QDateEdit {
            border: 1px solid #c7ddd8;
            border-radius: 10px;
            padding: 7px 10px;
            background: #ffffff;
            min-width: 118px;
        }
    )");

    connect(refreshButton, &QPushButton::clicked, this, &DashboardPage::refresh);
    connect(exportButton, &QPushButton::clicked, this, &DashboardPage::exportDashboardCsv);
    connect(m_apiClient, &ApiClient::responseReceived, this, &DashboardPage::onResponseReceived);
    QTimer::singleShot(0, this, &DashboardPage::refresh);
}

void DashboardPage::refresh()
{
    common::Request request;
    request.module = "dashboard";
    request.action = "summary";
    request.payload["startDate"] = m_startDateEdit->date().toString(Qt::ISODate);
    request.payload["endDate"] = m_endDateEdit->date().toString(Qt::ISODate);
    m_apiClient->send(request);
}

void DashboardPage::onResponseReceived(const common::Response& response)
{
    if (response.data.value("module").toString() != "dashboard"
        || response.data.value("action").toString() != "summary"
        || !response.success) {
        return;
    }

    m_todayRegistrations->setText(response.data.value("todayRegistrations").toVariant().toString());
    m_waitingPatients->setText(response.data.value("waitingPatients").toVariant().toString());
    m_finishedPatients->setText(response.data.value("finishedPatients").toVariant().toString());
    m_todayIncome->setText(QString::number(response.data.value("todayIncome").toVariant().toDouble(), 'f', 2));
    m_incomeMix->setText(QString("挂号 %1 / 药品 %2")
        .arg(response.data.value("registrationIncome").toVariant().toDouble(), 0, 'f', 2)
        .arg(response.data.value("drugIncome").toVariant().toDouble(), 0, 'f', 2));
    m_dailyVisits = response.data.value("dailyVisits").toArray();
    m_departmentVisits = response.data.value("departmentVisits").toArray();
    m_doctorRanking = response.data.value("doctorRanking").toArray();
    m_stockWarnings = response.data.value("stockWarnings").toArray();
    static_cast<DashboardChartWidget*>(m_dailyVisitsChart)->setRows(m_dailyVisits);
    static_cast<DashboardChartWidget*>(m_departmentVisitsChart)->setRows(m_departmentVisits);
    static_cast<DashboardChartWidget*>(m_doctorRankingChart)->setRows(m_doctorRanking);
    updateDoctorTable(response.data.value("doctorTop").toArray());
    updateWarningTable(m_stockWarnings);
}

void DashboardPage::updateWarningTable(const QJsonArray& rows)
{
    fillTable(m_warningTable, rows, {"药品编码", "药品名称", "库存", "预警库存"});
}

void DashboardPage::updateDoctorTable(const QJsonArray& rows)
{
    fillTable(m_doctorTable, rows, {"医生", "接诊量"});
}

void DashboardPage::exportDashboardCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        "导出驾驶舱报表",
        QString("驾驶舱报表_%1_%2.csv")
            .arg(m_startDateEdit->date().toString("yyyyMMdd"))
            .arg(m_endDateEdit->date().toString("yyyyMMdd")),
        "CSV 文件 (*.csv)");
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
    out << csvEscape("驾驶舱报表") << "\n";
    out << csvEscape("开始日期") << ',' << csvEscape(m_startDateEdit->date().toString(Qt::ISODate))
        << ',' << csvEscape("结束日期") << ',' << csvEscape(m_endDateEdit->date().toString(Qt::ISODate)) << "\n\n";
    out << csvEscape("日门诊量趋势") << "\n" << csvEscape("统计日期") << ',' << csvEscape("门诊量") << "\n";
    for (const auto& item : m_dailyVisits) {
        const auto row = item.toObject();
        out << csvEscape(row.value("统计日期").toVariant().toString()) << ','
            << csvEscape(row.value("门诊量").toVariant().toString()) << '\n';
    }
    out << "\n" << csvEscape("各科室接诊量") << "\n" << csvEscape("科室") << ',' << csvEscape("接诊量") << "\n";
    for (const auto& item : m_departmentVisits) {
        const auto row = item.toObject();
        out << csvEscape(row.value("科室").toVariant().toString()) << ','
            << csvEscape(row.value("接诊量").toVariant().toString()) << '\n';
    }
    out << "\n" << csvEscape("医生接诊排行") << "\n" << csvEscape("医生") << ',' << csvEscape("接诊量") << "\n";
    for (const auto& item : m_doctorRanking) {
        const auto row = item.toObject();
        out << csvEscape(row.value("医生").toVariant().toString()) << ','
            << csvEscape(row.value("接诊量").toVariant().toString()) << '\n';
    }
    out << "\n" << csvEscape("库存预警") << "\n"
        << csvEscape("药品编码") << ',' << csvEscape("药品名称") << ','
        << csvEscape("库存") << ',' << csvEscape("预警库存") << "\n";
    for (const auto& item : m_stockWarnings) {
        const auto row = item.toObject();
        out << csvEscape(row.value("药品编码").toVariant().toString()) << ','
            << csvEscape(row.value("药品名称").toVariant().toString()) << ','
            << csvEscape(row.value("库存").toVariant().toString()) << ','
            << csvEscape(row.value("预警库存").toVariant().toString()) << '\n';
    }

    QMessageBox::information(this, "导出成功", "驾驶舱报表已导出。");
}

} // namespace hospital::client

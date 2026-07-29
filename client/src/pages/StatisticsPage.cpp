#include "client/pages/Pages.h"

#include <QHash>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

namespace hospital::client {
namespace {

class DepartmentPieChart : public QWidget
{
public:
    explicit DepartmentPieChart(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(220);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setRows(const QJsonArray& rows)
    {
        m_values.clear();
        for (const auto& item : rows) {
            const auto row = item.toObject();
            const QString department = row.value("科室").toString();
            if (department.isEmpty() || department == "全院") {
                continue;
            }
            m_values[department] += row.value("总收入").toVariant().toDouble();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#ffffff"));

        painter.setPen(QColor("#12324a"));
        QFont titleFont = painter.font();
        titleFont.setBold(true);
        titleFont.setPointSize(11);
        painter.setFont(titleFont);
        painter.drawText(QRect(0, 12, width(), 28), Qt::AlignHCenter, "科室收入占比");

        double total = 0;
        for (auto it = m_values.cbegin(); it != m_values.cend(); ++it) {
            total += it.value();
        }

        if (total <= 0) {
            painter.setPen(QColor("#607080"));
            painter.drawText(rect().adjusted(0, 50, 0, 0), Qt::AlignCenter, "暂无统计数据");
            return;
        }

        const QList<QColor> colors = {
            QColor("#5675d6"), QColor("#2fb7a0"), QColor("#f0a23a"),
            QColor("#d95f76"), QColor("#6c8ea4"), QColor("#8c6dd7")
        };
        const QRect pieRect(width() / 2 - 78, 62, 156, 156);
        int startAngle = 90 * 16;
        int colorIndex = 0;
        int legendX = 30;
        int legendY = 58;

        for (auto it = m_values.cbegin(); it != m_values.cend(); ++it) {
            const QColor color = colors.at(colorIndex % colors.size());
            const int span = qRound(it.value() / total * 360.0 * 16.0);
            painter.setBrush(color);
            painter.setPen(Qt::NoPen);
            painter.drawPie(pieRect, startAngle, -span);
            startAngle -= span;

            painter.setBrush(color);
            painter.drawRoundedRect(legendX, legendY, 14, 8, 2, 2);
            painter.setPen(QColor("#394b5a"));
            painter.drawText(legendX + 22, legendY + 9,
                QString("%1  %2%").arg(it.key()).arg(it.value() / total * 100.0, 0, 'f', 1));
            legendY += 24;
            ++colorIndex;
        }
    }

private:
    QHash<QString, double> m_values;
};

} // namespace

StatisticsPage::StatisticsPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("费用统计", "按日期、科室和费用类型统计收入，为报表展示预留接口。", "statistics", "daily", apiClient, parent, 30000)
{
    auto* exportButton = new QPushButton("导出CSV报表", this);
    layout()->addWidget(exportButton);
    connect(exportButton, &QPushButton::clicked, this, &StatisticsPage::exportCsv);

    auto* chart = new DepartmentPieChart(this);
    chart->setObjectName("statisticsChart");
    m_chart = chart;
    layout()->addWidget(chart);
}

void StatisticsPage::rowsUpdated(const QJsonArray& rows)
{
    m_rows = rows;
    if (auto* chart = static_cast<DepartmentPieChart*>(m_chart)) {
        chart->setRows(rows);
    }
}

void StatisticsPage::exportCsv()
{
    if (m_rows.isEmpty()) {
        QMessageBox::information(this, "暂无数据", "当前没有可导出的统计数据。");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, "导出费用统计", "费用统计.csv", "CSV 文件 (*.csv)");
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
    out << "统计日期,科室,挂号收入,药品收入,总收入,药品占比\n";
    for (const auto& item : m_rows) {
        const auto row = item.toObject();
        out << row.value("统计日期").toVariant().toString() << ','
            << row.value("科室").toVariant().toString() << ','
            << row.value("挂号收入").toVariant().toString() << ','
            << row.value("药品收入").toVariant().toString() << ','
            << row.value("总收入").toVariant().toString() << ','
            << row.value("药品占比").toVariant().toString() << '\n';
    }
    QMessageBox::information(this, "导出成功", "统计报表已导出。");
}

} // namespace hospital::client

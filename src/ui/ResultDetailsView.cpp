#include "ui/ResultDetailsView.h"
#include "document/FlowsheetDocument.h"
#include "topology/TopologyGraph.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace afs {

ResultDetailsView::ResultDetailsView(FlowsheetDocument& document, QWidget* parent)
    : QFrame(parent), m_document(document), m_title(new QLabel(this)),
      m_table(new QTableWidget(this)) {
    setObjectName("resultDetails");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    m_title->setObjectName("resultDetailTitle");
    layout->addWidget(m_title);
    m_table->setObjectName("resultDetailTable");
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setMinimumHeight(190);
    layout->addWidget(m_table);
    showPlaceholder();
}

void ResultDetailsView::configureTable(int rows, int columns, const QStringList& headers) {
    m_table->clear();
    m_table->setRowCount(rows);
    m_table->setColumnCount(columns);
    m_table->setHorizontalHeaderLabels(headers);
}

void ResultDetailsView::setNumericItem(int row, int column, double value) {
    setTextItem(row, column, QString::number(value, 'f', 2));
}

void ResultDetailsView::setTextItem(int row, int column, const QString& value) {
    auto* item = new QTableWidgetItem(value);
    item->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, column, item);
}

void ResultDetailsView::showStream(const QString& streamId) {
    const auto* result = m_document.calculationResult();
    if (!result || !result->complete || !result->values.contains(streamId)) {
        showPlaceholder();
        return;
    }
    const auto value = result->values.value(streamId);
    m_title->setText(QString("物流结果 · %1").arg(streamId));
    const int componentCount = m_document.components().size();
    const bool hasResidual = result->reconciled && result->residuals.contains(streamId);
    configureTable(2 + 2 * componentCount + (hasResidual ? 2 : 0), 2,
                   {"指标", "数值"});
    QStringList names{"干质量", "全流程产率 / %"};
    for (const auto& component : m_document.components()) {
        names.append(QString("%1 品位 / %").arg(component.name));
        names.append(QString("%1 全流程回收率 / %").arg(component.name));
    }
    if (hasResidual) {
        names.append("质量协调修正");
        names.append("质量标准化残差 / σ");
    }
    for (int row = 0; row < names.size(); ++row) {
        m_table->setItem(row, 0, new QTableWidgetItem(names[row]));
    }
    setNumericItem(0, 1, value.dryMass);
    const auto overall = result->relativeToExternalFeed.constFind(streamId);
    if (overall == result->relativeToExternalFeed.cend()) setTextItem(1, 1, "—");
    else setNumericItem(1, 1, overall->massYieldPercent);
    int row = 2;
    for (int componentIndex = 0; componentIndex < m_document.components().size(); ++componentIndex) {
        const auto& component = m_document.components()[componentIndex];
        const auto componentResult = result->components.constFind(component.id);
        if (componentResult == result->components.cend()
            || !componentResult->values.contains(streamId)) {
            if (componentIndex == 0) {
                setNumericItem(row++, 1, value.gradePercent());
                if (overall == result->relativeToExternalFeed.cend()) setTextItem(row++, 1, "—");
                else setNumericItem(row++, 1, overall->recoveryPercent);
            } else {
                setTextItem(row++, 1, "—"); setTextItem(row++, 1, "—");
            }
            continue;
        }
        setNumericItem(row++, 1, componentResult->values.value(streamId).gradePercent());
        const auto metrics = componentResult->relativeToExternalFeed.constFind(streamId);
        if (metrics == componentResult->relativeToExternalFeed.cend()) setTextItem(row++, 1, "—");
        else setNumericItem(row++, 1, metrics->recoveryPercent);
    }
    if (hasResidual) {
        const auto residual = result->residuals.value(streamId);
        setNumericItem(row++, 1, residual.dryMass);
        setNumericItem(row++, 1, residual.dryMassStandardized);
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void ResultDetailsView::showUnit(const QString& unitId, const topology::TopologyGraph& graph) {
    const auto* result = m_document.calculationResult();
    const auto feeds = graph.streamsTo(unitId, topology::PortKind::Feed);
    const auto* node = graph.flotationNode(unitId);
    const auto ports = node ? topology::flotationProductPorts(*node)
                            : QVector<topology::PortKind>{};
    const auto productIds = graph.flotationProductStreams(unitId);
    if (!result || !result->complete || feeds.isEmpty() || !node
        || productIds.size() != ports.size()) {
        showPlaceholder();
        return;
    }
    QStringList ids{feeds.front()};
    QStringList headers{"指标", "入料"};
    for (int index = 0; index < productIds.size(); ++index) {
        ids.append(productIds[index]);
        switch (ports[index]) {
        case topology::PortKind::LeftProduct: headers.append("左产品"); break;
        case topology::PortKind::MiddleProduct: headers.append("中间产品"); break;
        case topology::PortKind::RightProduct: headers.append("右产品"); break;
        default: break;
        }
    }
    for (const auto& id : ids) if (!result->values.contains(id)) { showPlaceholder(); return; }

    m_title->setText(QString("浮选单元 %1 · 平衡结果").arg(unitId));
    configureTable(3 + 3 * m_document.components().size(), headers.size(), headers);
    QStringList names{"干质量", "节点产率 / %", "全流程产率 / %"};
    for (const auto& component : m_document.components()) {
        names.append(QString("%1 品位 / %").arg(component.name));
        names.append(QString("%1 节点回收率 / %").arg(component.name));
        names.append(QString("%1 全流程回收率 / %").arg(component.name));
    }
    for (int row = 0; row < names.size(); ++row)
        m_table->setItem(row, 0, new QTableWidgetItem(names[row]));
    for (int column = 0; column < ids.size(); ++column) {
        const auto value = result->values.value(ids[column]);
        const auto performance = result->flotationPerformance.value(unitId);
        const topology::ProductMetrics local = column == 0 ? topology::ProductMetrics{100, 100}
                                                           : performance.forPort(ports[column - 1]);
        setNumericItem(0, column + 1, value.dryMass);
        setNumericItem(1, column + 1, local.massYieldPercent);
        const auto overall = result->relativeToExternalFeed.constFind(ids[column]);
        if (overall == result->relativeToExternalFeed.cend()) setTextItem(2, column + 1, "—");
        else setNumericItem(2, column + 1, overall->massYieldPercent);
        int row = 3;
        for (int componentIndex = 0; componentIndex < m_document.components().size();
             ++componentIndex) {
            const auto& component = m_document.components()[componentIndex];
            const auto componentResult = result->components.constFind(component.id);
            if (componentResult == result->components.cend()
                || !componentResult->values.contains(ids[column])) {
                if (componentIndex == 0) {
                    setNumericItem(row++, column + 1, value.gradePercent());
                    setNumericItem(row++, column + 1, local.recoveryPercent);
                    if (overall == result->relativeToExternalFeed.cend())
                        setTextItem(row++, column + 1, "—");
                    else setNumericItem(row++, column + 1, overall->recoveryPercent);
                } else {
                    for (int i = 0; i < 3; ++i) setTextItem(row++, column + 1, "—");
                }
                continue;
            }
            setNumericItem(row++, column + 1,
                componentResult->values.value(ids[column]).gradePercent());
            const auto componentPerformance =
                componentResult->flotationPerformance.value(unitId);
            const topology::ProductMetrics componentLocal = column == 0
                ? topology::ProductMetrics{100, 100}
                : componentPerformance.forPort(ports[column - 1]);
            setNumericItem(row++, column + 1, componentLocal.recoveryPercent);
            const auto componentOverall =
                componentResult->relativeToExternalFeed.constFind(ids[column]);
            if (componentOverall == componentResult->relativeToExternalFeed.cend())
                setTextItem(row++, column + 1, "—");
            else setNumericItem(row++, column + 1, componentOverall->recoveryPercent);
        }
    }
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < headers.size(); ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
}

void ResultDetailsView::showCalculationExplanation(const topology::CalculationResult& result,
                                                   CalculationMode mode) {
    m_title->setText("计算说明");
    const int maximumDegreesOfFreedom = std::max(result.dryMassDegreesOfFreedom,
                                                  result.componentMassDegreesOfFreedom);
    QString state;
    if (result.complete) {
        state = result.fullySolved ? "已完成：全部物流已唯一确定"
            : "部分完成：已保留可唯一确定的物流，其余内部物流仍欠定";
    } else if (maximumDegreesOfFreedom > 0) {
        state = QString("尚欠定：至少还需要 %1 个独立约束").arg(maximumDegreesOfFreedom);
    } else {
        state = "未完成：请检查下方列出的拓扑或实测数据问题";
    }
    const QString balanceDescription = mode == CalculationMode::DataReconciliation
        ? "在物料守恒约束下，以标准差为权重最小化实测值修正；结果显示为协调值。"
        : "实测值作为严格约束，与各单元、汇流和贮池的质量守恒方程联立求解。";
    QString issues;
    for (const auto& issue : result.issues) {
        if (!issues.isEmpty()) issues.append('\n');
        issues.append(issue.message);
    }
    if (issues.isEmpty()) issues = "无";

    configureTable(6, 2, {"项目", "说明"});
    const QStringList labels{
        "计算模式", "求解变量", "守恒与观测约束", "当前状态", "自由度", "诊断"};
    const QStringList values{
        mode == CalculationMode::DataReconciliation ? "数据协调" : "严格模式",
        "每条物流分别以绝对干质量和各组分质量为变量；品位由组分质量 / 干质量换算。",
        balanceDescription,
        state,
        QString("干质量 %1；组分质量 %2")
            .arg(result.dryMassDegreesOfFreedom)
            .arg(result.componentMassDegreesOfFreedom),
        issues};
    for (int row = 0; row < labels.size(); ++row) {
        setTextItem(row, 0, labels[row]);
        setTextItem(row, 1, values[row]);
        m_table->item(row, 1)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    m_table->setWordWrap(true);
    m_table->resizeRowsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void ResultDetailsView::showPlaceholder() {
    m_title->setText("计算结果");
    configureTable(1, 1, {"提示"});
    auto* item = new QTableWidgetItem(m_document.calculationResult()
        ? "请选择产品线或浮选单元查看结果" : "完成所需物流数据后点击“计算”");
    item->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(0, 0, item);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

} // namespace afs

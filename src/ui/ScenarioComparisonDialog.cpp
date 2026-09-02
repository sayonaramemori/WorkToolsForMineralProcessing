#include "ui/ScenarioComparisonDialog.h"

#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace afs {

ScenarioComparisonDialog::ScenarioComparisonDialog(
    const FlowsheetDocument& document, const CanvasTopologySnapshot& snapshot, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("试验方案对比"));
    resize(760, 430);
    auto* layout = new QVBoxLayout(this);
    auto* selector = new QComboBox(this);
    for (const auto& product : snapshot.reportStreams) {
        const QString productName = document.productName(product.streamId).simplified();
        const QString label = productName.isEmpty()
            ? product.displayName
            : QString("%1（%2）").arg(productName, product.displayName);
        selector->addItem(label, product.streamId);
    }
    layout->addWidget(selector);

    auto* productName = new QLabel(this);
    layout->addWidget(productName);

    auto* table = new QTableWidget(this);
    QStringList headers{tr("试验方案"), tr("干质量"), tr("产率 / %")};
    for (const auto& component : document.components()) {
        headers.append(tr("%1 品位 / %").arg(component.name));
        headers.append(tr("%1 回收率 / %").arg(component.name));
    }
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table);

    const auto refresh = [&document, selector, productName, table] {
        const QString streamId = selector->currentData().toString();
        const QString name = document.productName(streamId).simplified();
        productName->setText(tr("产品名称：%1").arg(name.isEmpty() ? tr("未填写") : name));
        table->setRowCount(document.scenarios().size());
        for (int row = 0; row < document.scenarios().size(); ++row) {
            const auto& scenario = document.scenarios()[row];
            table->setItem(row, 0, new QTableWidgetItem(scenario.name));
            const auto* result = scenario.calculationResult ? &*scenario.calculationResult : nullptr;
            if (!result || !result->values.contains(streamId)) {
                for (int column = 1; column < table->columnCount(); ++column)
                    table->setItem(row, column, new QTableWidgetItem(tr("未计算")));
                continue;
            }
            const auto value = result->values.constFind(streamId);
            table->setItem(row, 1, new QTableWidgetItem(QString::number(value->dryMass, 'f', 2)));
            const auto metrics = result->relativeToExternalFeed.constFind(streamId);
            const bool hasMetrics = metrics != result->relativeToExternalFeed.cend();
            table->setItem(row, 2, new QTableWidgetItem(
                hasMetrics ? QString::number(metrics->massYieldPercent, 'f', 2) : QString("—")));
            int column = 3;
            for (int componentIndex = 0; componentIndex < document.components().size();
                 ++componentIndex) {
                const auto& component = document.components()[componentIndex];
                const auto componentResult = result->components.constFind(component.id);
                if (componentResult == result->components.cend()
                    || !componentResult->values.contains(streamId)) {
                    table->setItem(row, column++, new QTableWidgetItem(componentIndex == 0
                        ? QString::number(value->gradePercent(), 'f', 2) : QString("—")));
                    table->setItem(row, column++, new QTableWidgetItem(
                        componentIndex == 0 && hasMetrics
                            ? QString::number(metrics->recoveryPercent, 'f', 2) : QString("—")));
                    continue;
                }
                table->setItem(row, column++, new QTableWidgetItem(QString::number(
                    componentResult->values.value(streamId).gradePercent(), 'f', 2)));
                const auto componentMetrics = componentResult->relativeToExternalFeed.constFind(streamId);
                table->setItem(row, column++, new QTableWidgetItem(
                    componentMetrics == componentResult->relativeToExternalFeed.cend()
                        ? QString("—")
                        : QString::number(componentMetrics->recoveryPercent, 'f', 2)));
            }
        }
    };
    connect(selector, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [refresh](int) { refresh(); });
    refresh();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

} // namespace afs

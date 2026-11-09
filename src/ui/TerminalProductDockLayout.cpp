#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductViewSupport.h"
#include "document/ExperimentScenario.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace afs {

void TerminalProductDock::setupPanel() {
    setObjectName("terminalProductDock");
    m_panel->setObjectName("terminalProductPanel");
    auto* layout = new QVBoxLayout(m_panel);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    auto* summary = new QFrame(m_panel);
    summary->setObjectName("terminalSummary");
    auto* summaryLayout = new QVBoxLayout(summary);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    summaryLayout->setSpacing(8);
    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(12);
    auto* title = new QLabel("实验数据", summary);
    title->setObjectName("terminalSummaryTitle");
    auto* hint = new QLabel("双击质量或各组分品位进行编辑", summary);
    hint->setObjectName("terminalSummaryHint");
    hint->setToolTip(hint->text());
    hint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    titleRow->addWidget(title);
    titleRow->addWidget(hint, 1);
    m_progressLabel->setObjectName("terminalProgress");
    m_progressLabel->setAlignment(Qt::AlignCenter);
    titleRow->addWidget(m_progressLabel);
    summaryLayout->addLayout(titleRow);
    m_calculateButton->setObjectName("calculateButton");
    m_calculateButton->setMinimumHeight(32);
    m_explanationButton->setObjectName("calculationExplanationButton");
    m_explanationButton->setMinimumHeight(32);
    auto* componentsButton = new QPushButton("组分设置", summary);
    componentsButton->setObjectName("componentSettingsButton");
    componentsButton->setMinimumHeight(32);
    connect(componentsButton, &QPushButton::clicked, this, &TerminalProductDock::editComponents);

    m_calculationStatus->setObjectName("calculationStatus");
    m_calculationStatus->setWordWrap(true);
    auto* filterRow = new QHBoxLayout;
    filterRow->setSpacing(8);
    filterRow->addWidget(new QLabel("显示", summary));
    m_filterCombo->setObjectName("streamFilterCombo");
    m_filterCombo->setMinimumHeight(32);
    m_filterCombo->addItem("全部物流", StreamFilterProxyModel::All);
    m_filterCombo->addItem("已填写", StreamFilterProxyModel::Entered);
    m_filterCombo->addItem("待补充", StreamFilterProxyModel::NeedsInput);
    m_filterCombo->addItem("终端产品", StreamFilterProxyModel::Terminal);
    m_filterCombo->addItem("浮选入料", StreamFilterProxyModel::Feed);
    m_filterCombo->addItem("回流", StreamFilterProxyModel::Recycle);
    m_filterCombo->addItem("选中流程关联", StreamFilterProxyModel::SelectedUnit);
    filterRow->addWidget(m_filterCombo);
    filterRow->addSpacing(12);
    filterRow->addWidget(new QLabel("计算模式", summary));
    m_modeCombo->setObjectName("calculationModeCombo");
    m_modeCombo->setMinimumHeight(32);
    m_modeCombo->addItem("严格模式", static_cast<int>(CalculationMode::Strict));
    m_modeCombo->addItem("数据协调", static_cast<int>(CalculationMode::DataReconciliation));
    filterRow->addWidget(m_modeCombo);
    filterRow->addStretch();
    filterRow->addWidget(m_explanationButton);
    filterRow->addWidget(componentsButton);
    filterRow->addWidget(m_calculateButton);
    summaryLayout->addLayout(filterRow);
    summaryLayout->addWidget(m_calculationStatus);
    layout->addWidget(summary);
    layout->addWidget(m_table, 1);

    m_table->setObjectName("terminalProductTable");
    m_filterModel->setSourceModel(m_model);
    m_filterModel->setDynamicSortFilter(true);
    m_table->setModel(m_filterModel);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_table->setShowGrid(true);
    m_table->setGridStyle(Qt::SolidLine);
    m_table->setAlternatingRowColors(true);
    m_table->setCornerButtonEnabled(false);
    m_table->installEventFilter(this);
    m_table->viewport()->installEventFilter(this);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(44);
    m_table->horizontalHeader()->setMinimumHeight(38);
    m_table->horizontalHeader()->setHighlightSections(false);
    configureColumns();
    connect(m_table->horizontalHeader(), &QHeaderView::sectionResized, this, [this] {
        if (!m_applyingColumnWidths) m_columnsManuallyResized = true;
    });

    setWidget(m_panel);
    setMinimumWidth(700);
}

} // namespace afs

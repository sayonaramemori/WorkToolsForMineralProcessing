#include "ui/TerminalProductDock.h"
#include "document/FlowsheetDocument.h"
#include "ui/ResultDetailsView.h"
#include "ui/TerminalProductStyle.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductViewSupport.h"
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableView>
#include <QVBoxLayout>
#include <utility>

namespace afs {

TerminalProductDock::TerminalProductDock(FlowsheetDocument& document, QWidget* parent)
    : QDockWidget("物流实测参数", parent), m_table(new QTableView(this)),
      m_model(new TerminalProductTableModel(document, this)),
      m_filterModel(new StreamFilterProxyModel(this)), m_filterCombo(new QComboBox(this)),
      m_modeCombo(new QComboBox(this)),
      m_interestButton(new QPushButton("关注对象：全部", this)),
      m_interestMenu(new QMenu(this)),
      m_progressLabel(new QLabel(this)),
      m_panel(new QWidget(this)), m_document(document), m_calculateButton(new QPushButton("计算", this)),
      m_explanationButton(new QPushButton("计算说明", this)),
      m_calculationStatus(new QLabel(this)), m_resultDetails(new ResultDetailsView(document, this)) {
    setObjectName("terminalProductDock");
    m_panel->setObjectName("terminalProductPanel");
    auto* layout = new QVBoxLayout(m_panel);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    auto* summary = new QFrame(m_panel);
    summary->setObjectName("terminalSummary");
    auto* summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    auto* labels = new QVBoxLayout;
    auto* title = new QLabel("实验数据", summary);
    title->setObjectName("terminalSummaryTitle");
    auto* hint = new QLabel("双击质量或各组分品位进行编辑", summary);
    hint->setObjectName("terminalSummaryHint");
    labels->addWidget(title);
    labels->addWidget(hint);
    summaryLayout->addLayout(labels, 1);
    m_progressLabel->setObjectName("terminalProgress");
    m_progressLabel->setAlignment(Qt::AlignCenter);
    summaryLayout->addWidget(m_progressLabel);
    m_calculateButton->setObjectName("calculateButton");
    m_calculateButton->setMinimumHeight(32);
    summaryLayout->addWidget(m_calculateButton);
    m_explanationButton->setObjectName("calculationExplanationButton");
    m_explanationButton->setMinimumHeight(32);
    summaryLayout->addWidget(m_explanationButton);
    auto* componentsButton = new QPushButton("组分设置", summary);
    componentsButton->setMinimumHeight(32);
    summaryLayout->addWidget(componentsButton);
    layout->addWidget(summary);

    m_calculationStatus->setObjectName("calculationStatus");
    m_calculationStatus->setWordWrap(true);
    layout->addWidget(m_calculationStatus);
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel("显示", m_panel));
    m_filterCombo->setObjectName("streamFilterCombo");
    m_filterCombo->addItem("全部物流", StreamFilterProxyModel::All);
    m_filterCombo->addItem("已填写", StreamFilterProxyModel::Entered);
    m_filterCombo->addItem("待补充", StreamFilterProxyModel::NeedsInput);
    m_filterCombo->addItem("终端产品", StreamFilterProxyModel::Terminal);
    m_filterCombo->addItem("浮选入料", StreamFilterProxyModel::Feed);
    m_filterCombo->addItem("回流", StreamFilterProxyModel::Recycle);
    m_filterCombo->addItem("选中流程关联", StreamFilterProxyModel::SelectedUnit);
    filterRow->addWidget(m_filterCombo);
    m_interestButton->setObjectName("interestObjectButton");
    m_interestButton->setMenu(m_interestMenu);
    filterRow->addWidget(m_interestButton);
    filterRow->addSpacing(12);
    filterRow->addWidget(new QLabel("计算模式", m_panel));
    m_modeCombo->setObjectName("calculationModeCombo");
    m_modeCombo->addItem("严格模式", static_cast<int>(CalculationMode::Strict));
    m_modeCombo->addItem("数据协调", static_cast<int>(CalculationMode::DataReconciliation));
    filterRow->addWidget(m_modeCombo);
    filterRow->addStretch();
    layout->addLayout(filterRow);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_resultDetails);

    m_table->setObjectName("terminalProductTable");
    m_filterModel->setSourceModel(m_model);
    m_filterModel->setDynamicSortFilter(true);
    m_table->setModel(m_filterModel);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setCornerButtonEnabled(false);
    m_table->installEventFilter(this);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(44);
    m_table->horizontalHeader()->setMinimumHeight(38);
    m_table->horizontalHeader()->setHighlightSections(false);
    configureColumns();

    setWidget(m_panel);
    setMinimumWidth(700);
    applyPanelStyle();
    updateSummary();
    updateCalculationState();

    connect(m_table, &QTableView::clicked, this, [this](const QModelIndex& index) {
        const QModelIndex source = m_filterModel->mapToSource(index);
        if (const auto* stream = m_model->streamAt(source.row()))
            emit graphicsItemRequested(stream->graphicsItem);
    });
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this] {
        updateSummary(); updateCalculationState();
    });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this] {
        updateSummary(); updateCalculationState();
    });
    connect(m_calculateButton, &QPushButton::clicked, this, &TerminalProductDock::calculationRequested);
    connect(m_explanationButton, &QPushButton::clicked,
            this, &TerminalProductDock::showCalculationExplanation);
    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_filterModel->setMode(static_cast<StreamFilterProxyModel::Mode>(
            m_filterCombo->itemData(index).toInt()));
        updateSummary();
    });
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) m_document.setCalculationMode(
            static_cast<CalculationMode>(m_modeCombo->itemData(index).toInt()));
    });
    connect(componentsButton, &QPushButton::clicked, this, &TerminalProductDock::editComponents);
    connect(&m_document, &FlowsheetDocument::componentsChanged, this, [this] {
        configureColumns(); updateSummary(); updateCalculationState();
    });
    connect(&m_document, &FlowsheetDocument::calculationChanged, this, [this] {
        updateCalculationState();
        if (!m_document.calculationResult()) m_resultDetails->showPlaceholder();
    });
    connect(&m_document, &FlowsheetDocument::calculationModeChanged, this, [this] {
        const int index = m_modeCombo->findData(static_cast<int>(m_document.calculationMode()));
        if (index >= 0 && index != m_modeCombo->currentIndex()) {
            const QSignalBlocker blocker(m_modeCombo);
            m_modeCombo->setCurrentIndex(index);
        }
        configureColumns(); updateSummary(); updateCalculationState();
    });
}

void TerminalProductDock::configureColumns() {
    auto* header = m_table->horizontalHeader();
    // Experimental flowsheets often need either a compact data view or long
    // stream names. Every section is therefore manually resizable instead of
    // locking numeric columns and stretching the first column.
    header->setMinimumSectionSize(48);
    header->setStretchLastSection(false);
    for (int column = 0; column < m_model->columnCount(); ++column)
        header->setSectionResizeMode(column, QHeaderView::Interactive);
    m_table->setColumnWidth(TerminalProductTableModel::NameColumn, 210);
    m_table->setColumnWidth(TerminalProductTableModel::ProductNameColumn, 110);
    m_table->setColumnWidth(TerminalProductTableModel::MassColumn, 82);
    m_table->setItemDelegateForColumn(TerminalProductTableModel::MassColumn,
                                      new NumberDelegate(1.0e12, m_table));
    if (m_document.calculationMode() == CalculationMode::DataReconciliation) {
        m_table->setColumnWidth(TerminalProductTableModel::MassStdDevColumn, 72);
        m_table->setItemDelegateForColumn(TerminalProductTableModel::MassStdDevColumn,
                                          new NumberDelegate(1.0e12, m_table));
    }
    for (const auto& component : m_document.components()) {
        const int grade = m_model->gradeColumn(component.id);
        m_table->setColumnWidth(grade, 88);
        m_table->setItemDelegateForColumn(grade, new NumberDelegate(100.0, m_table));
        const int sigma = m_model->gradeStdDevColumn(component.id);
        if (sigma >= 0) {
            m_table->setColumnWidth(sigma, 72);
            m_table->setItemDelegateForColumn(sigma, new NumberDelegate(100.0, m_table));
        }
    }
    m_table->setColumnWidth(m_model->statusColumn(), 82);
    m_table->setItemDelegateForColumn(m_model->statusColumn(), new StatusDelegate(m_table));
}

bool TerminalProductDock::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_table && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Delete) {
            const QModelIndex current = m_table->currentIndex();
            if (current.isValid() && current.flags().testFlag(Qt::ItemIsEditable)) {
                m_table->model()->setData(current, QVariant(), Qt::EditRole);
                return true;
            }
        }
    }
    return QDockWidget::eventFilter(watched, event);
}

void TerminalProductDock::editComponents() {
    QStringList names;
    for (const auto& component : m_document.components()) names.append(component.name);
    bool accepted = false;
    const QString text = QInputDialog::getMultiLineText(
        this, tr("组分设置"), tr("每行填写一个组分名称（例如 Cu、Pb、Zn）"),
        names.join('\n'), &accepted);
    if (!accepted) return;
    QStringList requested;
    for (const auto& line : text.split('\n')) {
        const QString name = line.simplified().left(40);
        if (!name.isEmpty() && !requested.contains(name)) requested.append(name);
    }
    if (requested.isEmpty()) return;
    QVector<ComponentDefinition> components;
    for (int i = 0; i < requested.size(); ++i) {
        const QString id = i < m_document.components().size()
            ? m_document.components()[i].id : QString("component-%1").arg(i + 1);
        components.append({id, requested[i]});
    }
    m_document.setComponents(std::move(components));
}

void TerminalProductDock::applyPanelStyle() {
    m_panel->setStyleSheet(TerminalProductStyle::styleSheet(qApp->palette()));
    m_panel->update();
}

void TerminalProductDock::refreshAppearance() {
    applyPanelStyle();
    m_table->viewport()->update();
}

} // namespace afs

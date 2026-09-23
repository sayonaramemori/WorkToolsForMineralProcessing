#include "ui/TerminalProductDock.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "ui/ResultDetailsView.h"
#include "ui/TerminalProductStyle.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductViewSupport.h"
#include "services/FlowsheetCalculationService.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QApplication>
#include <QDoubleValidator>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QInputDialog>
#include <QPainter>
#include <QPushButton>
#include <QComboBox>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QSet>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QTableView>
#include <QVBoxLayout>

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

void TerminalProductDock::setSnapshot(CanvasTopologySnapshot snapshot) {
    m_snapshot = std::move(snapshot);
    QSet<QString> editableIds;
    for (const auto& stream : m_snapshot.reportStreams) editableIds.insert(stream.streamId);
    m_model->setStreams(m_snapshot.reportStreams, std::move(editableIds));
    rebuildInterestMenu();
}

void TerminalProductDock::rebuildInterestMenu() {
    m_interestMenu->clear();
    QSet<QString> validSelection;
    for (const auto& object : m_snapshot.interestObjects) {
        auto* action = m_interestMenu->addAction(object.displayName);
        action->setCheckable(true);
        action->setData(object.id);
        if (m_interestedOwners.contains(object.id)) {
            action->setChecked(true);
            validSelection.insert(object.id);
        }
        connect(action, &QAction::toggled, this, [this] {
            QSet<QString> selected;
            int count = 0;
            for (auto* candidate : m_interestMenu->actions()) {
                if (!candidate->isChecked()) continue;
                selected.insert(candidate->data().toString());
                ++count;
            }
            m_interestedOwners = selected;
            m_filterModel->setInterestedOwners(std::move(selected));
            m_interestButton->setText(count == 0
                ? "关注对象：全部" : QString("关注对象：%1 个").arg(count));
            updateSummary();
        });
    }
    if (m_snapshot.interestObjects.isEmpty()) {
        auto* empty = m_interestMenu->addAction("暂无可选对象");
        empty->setEnabled(false);
    }
    m_interestedOwners = std::move(validSelection);
    m_filterModel->setInterestedOwners(m_interestedOwners);
    m_interestButton->setText(m_interestedOwners.isEmpty()
        ? "关注对象：全部" : QString("关注对象：%1 个").arg(m_interestedOwners.size()));
}

void TerminalProductDock::selectGraphicsItem(const QGraphicsItem* item) {
    const int row = m_model->rowForGraphicsItem(item);
    if (row < 0) { m_table->clearSelection(); return; }
    const QModelIndex current = m_filterModel->mapToSource(m_table->currentIndex());
    if (current.isValid() && current.row() == row) return;
    const QModelIndex source = m_model->index(row, TerminalProductTableModel::MassColumn);
    const QModelIndex target = m_filterModel->mapFromSource(source);
    if (!target.isValid()) { m_table->clearSelection(); return; }
    m_table->selectRow(target.row());
    m_table->setCurrentIndex(target);
    m_table->scrollTo(target);
}

void TerminalProductDock::refreshAppearance() {
    applyPanelStyle();
    m_table->viewport()->update();
}

void TerminalProductDock::showStreamResult(const QString& streamId) {
    m_resultDetails->showStream(streamId);
}

void TerminalProductDock::showUnitResult(const QString& unitId, const topology::TopologyGraph& graph) {
    m_resultDetails->showUnit(unitId, graph);
}

void TerminalProductDock::clearResultDetails() { m_resultDetails->showPlaceholder(); }

void TerminalProductDock::setScenarioName(const QString& name) {
    setWindowTitle(QString("物流实测参数 · %1").arg(name));
}

void TerminalProductDock::updateSummary() {
    const int complete = m_model->completedCount();
    m_progressLabel->setText(m_model->rowCount() == 0
        ? "暂无物流" : QString("显示 %1/%2 · 已录入 %3")
              .arg(m_filterModel->rowCount()).arg(m_model->rowCount()).arg(complete));
}

void TerminalProductDock::updateCalculationState() {
    const bool hasCompleteInput = m_model->completedCount() > 0;
    m_calculateButton->setEnabled(hasCompleteInput);
    const auto* result = m_document.calculationResult();
    if (result && result->complete) {
        if (result->reconciled) {
            m_calculationStatus->setText(QString("数据协调完成 · 最大标准化残差 %1σ%2")
                .arg(result->maximumAbsoluteStandardizedResidual, 0, 'f', 2)
                .arg(result->maximumAbsoluteStandardizedResidual > 3.0
                    ? " · 存在可疑测量" : ""));
        } else m_calculationStatus->setText(result->fullySolved
            ? "平衡计算成功 · 选择画布对象查看详细结果"
            : "全流程平衡已完成 · 部分中间物流未唯一求解");
        m_calculateButton->setText("重新计算");
    } else if (result) {
        const int missing = std::max(result->dryMassDegreesOfFreedom,
                                     result->componentMassDegreesOfFreedom);
        const bool conflict = std::any_of(result->issues.cbegin(), result->issues.cend(),
            [](const auto& issue) {
                return issue.code == topology::IssueCode::InconsistentBalance
                    || issue.code == topology::IssueCode::InvalidMeasurement;
            });
        m_calculationStatus->setText(conflict
            ? QString("计算未完成 · 输入数据或支路占比相互矛盾")
            : missing > 0
                ? QString("计算未完成 · 方程组仍有 %1 个独立自由度，请补充实测物流")
                      .arg(missing)
                : QString("计算未完成 · %1 个流程问题").arg(result->issues.size()));
        m_calculateButton->setText("重新计算");
    } else {
        const auto preview = FlowsheetCalculationService::calculate(
            m_snapshot, m_document);
        const int missing = std::max(preview.dryMassDegreesOfFreedom,
                                     preview.componentMassDegreesOfFreedom);
        m_calculationStatus->setText(m_model->rowCount() == 0
            ? "请先在画布中建立流程"
            : missing > 0
                ? QString("当前方程组还有 %1 个独立自由度 · 可补充任意实测物流")
                      .arg(missing)
                : hasCompleteInput
                    ? "当前约束已足够，可以计算"
                    : "可填写总入料或任意产品物流的质量和各组分品位");
        m_calculateButton->setText("计算");
    }
}

void TerminalProductDock::showCalculationExplanation() {
    // Once an input changes, its stored result is invalidated.  Use a preview
    // in that case so the explanation always describes the values on screen.
    const auto* stored = m_document.calculationResult();
    const auto preview = stored ? topology::CalculationResult{}
                                : FlowsheetCalculationService::calculate(m_snapshot, m_document);
    const auto& result = stored ? *stored : preview;
    m_resultDetails->showCalculationExplanation(result, m_document.calculationMode());
}

} // namespace afs

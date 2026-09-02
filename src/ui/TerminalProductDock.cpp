#include "ui/TerminalProductDock.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "ui/ResultDetailsView.h"
#include "ui/TerminalProductStyle.h"
#include "ui/TerminalProductTableModel.h"

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
#include <QSet>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QTableView>
#include <QVBoxLayout>

namespace afs {
namespace {

class NumberDelegate final : public QStyledItemDelegate {
public:
    NumberDelegate(double maximum, QObject* parent) : QStyledItemDelegate(parent), m_maximum(maximum) {}
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override {
        auto* editor = new QLineEdit(parent);
        auto* validator = new QDoubleValidator(0.0, m_maximum, 6, editor);
        validator->setNotation(QDoubleValidator::StandardNotation);
        editor->setValidator(validator);
        editor->setAlignment(Qt::AlignCenter);
        editor->setFrame(false);
        return editor;
    }
    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        auto* lineEdit = static_cast<QLineEdit*>(editor);
        lineEdit->setText(index.data(Qt::EditRole).toString());
        lineEdit->selectAll();
    }
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        model->setData(index, static_cast<QLineEdit*>(editor)->text(), Qt::EditRole);
    }
private:
    double m_maximum;
};

class StatusDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem base(option);
        initStyleOption(&base, index);
        base.text.clear();
        QStyledItemDelegate::paint(painter, base, index);
        const QString text = index.data(Qt::DisplayRole).toString();
        const bool ready = text == QStringLiteral("已填写")
            || text == QStringLiteral("计算值");
        const QColor foreground = ready ? QColor("#17864b") : QColor("#a86400");
        QColor background = ready ? QColor("#dff5e8") : QColor("#fff0d6");
        if (option.state & QStyle::State_Selected) {
            background = option.palette.color(QPalette::HighlightedText);
            background.setAlpha(225);
        }
        const int width = QFontMetrics(option.font).horizontalAdvance(text) + 18;
        const QRect badge(option.rect.center().x() - width / 2, option.rect.center().y() - 12, width, 24);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(background);
        painter->drawRoundedRect(badge, 12, 12);
        painter->setPen(foreground);
        painter->drawText(badge, Qt::AlignCenter, text);
        painter->restore();
    }
};

} // namespace

TerminalProductDock::TerminalProductDock(FlowsheetDocument& document, QWidget* parent)
    : QDockWidget("物流实测参数", parent), m_table(new QTableView(this)),
      m_model(new TerminalProductTableModel(document, this)), m_progressLabel(new QLabel(this)),
      m_panel(new QWidget(this)), m_document(document), m_calculateButton(new QPushButton("计算", this)),
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
    auto* componentsButton = new QPushButton("组分设置", summary);
    componentsButton->setMinimumHeight(32);
    summaryLayout->addWidget(componentsButton);
    layout->addWidget(summary);

    m_calculationStatus->setObjectName("calculationStatus");
    m_calculationStatus->setWordWrap(true);
    layout->addWidget(m_calculationStatus);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_resultDetails);

    m_table->setObjectName("terminalProductTable");
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setCornerButtonEnabled(false);
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
        if (const auto* stream = m_model->streamAt(index.row()))
            emit graphicsItemRequested(stream->graphicsItem);
    });
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this] {
        updateSummary(); updateCalculationState();
    });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this] {
        updateSummary(); updateCalculationState();
    });
    connect(m_calculateButton, &QPushButton::clicked, this, &TerminalProductDock::calculationRequested);
    connect(componentsButton, &QPushButton::clicked, this, &TerminalProductDock::editComponents);
    connect(&m_document, &FlowsheetDocument::componentsChanged, this, [this] {
        configureColumns(); updateSummary(); updateCalculationState();
    });
    connect(&m_document, &FlowsheetDocument::calculationChanged, this, [this] {
        updateCalculationState();
        if (!m_document.calculationResult()) m_resultDetails->showPlaceholder();
    });
}

void TerminalProductDock::configureColumns() {
    auto* header = m_table->horizontalHeader();
    header->setSectionResizeMode(TerminalProductTableModel::NameColumn, QHeaderView::Stretch);
    for (int column = 1; column < m_model->columnCount(); ++column)
        header->setSectionResizeMode(column, QHeaderView::Fixed);
    m_table->setColumnWidth(TerminalProductTableModel::ProductNameColumn, 110);
    m_table->setColumnWidth(TerminalProductTableModel::MassColumn, 82);
    m_table->setItemDelegateForColumn(TerminalProductTableModel::MassColumn,
                                      new NumberDelegate(1.0e12, m_table));
    for (const auto& component : m_document.components()) {
        const int grade = m_model->gradeColumn(component.id);
        const int share = m_model->componentShareColumn(component.id);
        m_table->setColumnWidth(grade, 88);
        m_table->setColumnWidth(share, 96);
        m_table->setItemDelegateForColumn(grade, new NumberDelegate(100.0, m_table));
        m_table->setItemDelegateForColumn(share, new NumberDelegate(100.0, m_table));
    }
    m_table->setColumnWidth(m_model->dryMassShareColumn(), 92);
    m_table->setItemDelegateForColumn(m_model->dryMassShareColumn(),
                                      new NumberDelegate(100.0, m_table));
    m_table->setColumnWidth(m_model->statusColumn(), 82);
    m_table->setItemDelegateForColumn(m_model->statusColumn(), new StatusDelegate(m_table));
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
    QSet<QString> requiredIds;
    for (const auto& stream : snapshot.requiredMeasurements) requiredIds.insert(stream.streamId);
    m_model->setStreams(std::move(snapshot.reportStreams), std::move(requiredIds));
}

void TerminalProductDock::selectGraphicsItem(const QGraphicsItem* item) {
    const int row = m_model->rowForGraphicsItem(item);
    if (row < 0) { m_table->clearSelection(); return; }
    const QModelIndex target = m_model->index(row, TerminalProductTableModel::MassColumn);
    m_table->selectRow(row);
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
    const int total = m_model->requiredCount();
    const int complete = m_model->completedCount();
    m_progressLabel->setText(total == 0 ? "暂无物流" : QString("%1 / %2 完成").arg(complete).arg(total));
}

void TerminalProductDock::updateCalculationState() {
    const int total = m_model->requiredCount();
    const bool inputsComplete = total > 0 && m_model->completedCount() == total;
    m_calculateButton->setEnabled(inputsComplete);
    const auto* result = m_document.calculationResult();
    if (result && result->complete) {
        m_calculationStatus->setText(result->fullySolved
            ? "平衡计算成功 · 选择画布对象查看详细结果"
            : "全流程平衡已完成 · 部分中间物流未唯一求解");
        m_calculateButton->setText("重新计算");
    } else if (result) {
        m_calculationStatus->setText(QString("计算未完成 · %1 个问题").arg(result->issues.size()));
        m_calculateButton->setText("重新计算");
    } else {
        m_calculationStatus->setText(inputsComplete ? "所需物流数据已完整，可以开始计算"
                                                    : "请先填写终端产品、合流支路及回流支路的质量和各组分品位");
        m_calculateButton->setText("计算");
    }
}

} // namespace afs

#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductStyle.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductViewSupport.h"
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableView>
#include <utility>

namespace afs {

TerminalProductDock::TerminalProductDock(TerminalTableSession& session, QWidget* parent)
    : QDockWidget("物流实测参数", parent), m_table(new QTableView(this)),
      m_model(new TerminalProductTableModel(session, this)),
      m_filterModel(new StreamFilterProxyModel(this)), m_filterCombo(new QComboBox(this)),
      m_modeCombo(new QComboBox(this)),
      m_progressLabel(new QLabel(this)),
      m_panel(new QWidget(this)), m_session(session), m_calculateButton(new QPushButton("计算", this)),
      m_explanationButton(new QPushButton("计算说明", this)),
      m_calculationStatus(new QLabel(this)) {
    setupPanel();
    applyPanelStyle();
    updateSummary();
    setCalculationState({});

    connect(m_table, &QTableView::clicked, this, [this](const QModelIndex& index) {
        const QModelIndex source = m_filterModel->mapToSource(index);
        if (const auto* stream = m_model->streamAt(source.row()))
            emit streamRequested(stream->streamId);
    });
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this] {
        updateSummary();
    });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this] {
        updateSummary();
    });
    connect(m_calculateButton, &QPushButton::clicked, this, &TerminalProductDock::calculationRequested);
    connect(m_explanationButton, &QPushButton::clicked,
            this, &TerminalProductDock::calculationExplanationRequested);
    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_filterModel->setMode(static_cast<StreamFilterProxyModel::Mode>(
            m_filterCombo->itemData(index).toInt()));
        updateSummary();
    });
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) m_session.submit(EditTableMode{static_cast<CalculationMode>(m_modeCombo->itemData(index).toInt())});
    });
    connect(m_model, &TerminalProductTableModel::schemaChanged, this, [this] {
        const int index = m_modeCombo->findData(static_cast<int>(m_model->tableState().mode));
        if (index >= 0 && index != m_modeCombo->currentIndex()) {
            const QSignalBlocker blocker(m_modeCombo);
            m_modeCombo->setCurrentIndex(index);
        }
        configureColumns(); updateSummary();
    });
    {
        const QSignalBlocker blocker(m_modeCombo);
        m_modeCombo->setCurrentIndex(m_modeCombo->findData(static_cast<int>(m_model->tableState().mode)));
    }
}

bool TerminalProductDock::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == m_table->viewport() && event->type() == QEvent::Resize)
        || (watched == m_table && event->type() == QEvent::Show)) scheduleColumnFit();
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

void TerminalProductDock::applyPanelStyle() {
    m_panel->setStyleSheet(TerminalProductStyle::styleSheet(qApp->palette()));
    m_panel->update();
}

void TerminalProductDock::refreshAppearance() {
    applyPanelStyle();
    m_table->viewport()->update();
}

} // namespace afs

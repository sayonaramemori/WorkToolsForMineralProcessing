#include "app/MainWindow.h"

#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "ui/ScenarioComparisonDialog.h"
#include "ui/TerminalProductDock.h"

#include <QComboBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>

namespace afs {

void MainWindow::refreshScenarioUi() {
    if (!m_scenarioCombo) return;
    const QSignalBlocker blocker(m_scenarioCombo);
    m_scenarioCombo->clear();
    for (const auto& scenario : m_document->scenarios())
        m_scenarioCombo->addItem(scenario.name, scenario.id);
    m_scenarioCombo->setCurrentIndex(m_document->currentScenarioIndex());
    if (m_terminalDock) m_terminalDock->setScenarioName(m_document->currentScenarioName());
}

void MainWindow::addScenario(bool copyCurrent) {
    bool accepted = false;
    const QString suggested = copyCurrent
        ? tr("%1 - 副本").arg(m_document->currentScenarioName())
        : tr("方案 %1").arg(m_document->scenarios().size() + 1);
    const QString name = QInputDialog::getText(
        this, copyCurrent ? tr("复制试验方案") : tr("新增试验方案"),
        tr("方案名称"), QLineEdit::Normal, suggested, &accepted).simplified();
    if (!accepted || name.isEmpty()) return;
    const int index = m_document->addScenario(name, copyCurrent);
    if (index >= 0) m_document->setCurrentScenario(index);
}

void MainWindow::renameScenario() {
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("重命名试验方案"), tr("方案名称"), QLineEdit::Normal,
        m_document->currentScenarioName(), &accepted).simplified();
    if (accepted && !name.isEmpty())
        m_document->renameScenario(m_document->currentScenarioIndex(), name);
}

void MainWindow::deleteScenario() {
    if (m_document->scenarios().size() <= 1) {
        QMessageBox::information(this, tr("删除试验方案"), tr("项目至少需要保留一个试验方案。"));
        return;
    }
    if (QMessageBox::question(this, tr("删除试验方案"),
            tr("确定删除“%1”及其药剂和实验数据吗？").arg(
                m_document->currentScenarioName())) != QMessageBox::Yes) return;
    m_document->removeScenario(m_document->currentScenarioIndex());
}

void MainWindow::compareScenarios() {
    const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
    if (snapshot.terminalProducts.isEmpty()) {
        QMessageBox::information(this, tr("方案对比"), tr("当前流程没有可对比的终端产品。"));
        return;
    }
    ScenarioComparisonDialog(*m_document, snapshot, this).exec();
}

} // namespace afs

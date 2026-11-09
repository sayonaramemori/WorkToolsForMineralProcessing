#include "ui/CalculationIssuesDock.h"
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
namespace afs {
CalculationIssuesDock::CalculationIssuesDock(QWidget* parent) : QDockWidget("计算问题", parent) {
    setObjectName("calculationIssuesDock"); auto* body = new QWidget; auto* layout = new QVBoxLayout(body);
    m_status = new QLabel("点击“检查流程”查看问题；单击定位画布，双击切到表格输入。"); m_status->setWordWrap(true); layout->addWidget(m_status);
    m_tree = new QTreeWidget; m_tree->setObjectName("calculationIssueTree");
    m_tree->setHeaderLabels({"级别", "对象", "说明"}); m_tree->setRootIsDecorated(false); m_tree->setWordWrap(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setStretchLastSection(true); layout->addWidget(m_tree);
    m_details = new QLabel; m_details->setWordWrap(true); m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_details->setObjectName("calculationIssueDetails"); layout->addWidget(m_details);
    auto* check = new QPushButton("检查流程 / 重新计算"); check->setObjectName("checkFlowsheetButton"); layout->addWidget(check); setWidget(body);
    connect(check, &QPushButton::clicked, this, &CalculationIssuesDock::checkRequested);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        m_details->setText(item ? item->text(2) : QString{});
    });
    const auto locate = [this](QTreeWidgetItem* item, bool edit) {
        emit locateRequested(item->data(0, Qt::UserRole).toString(), item->data(1, Qt::UserRole).toString(),
            item->data(2, Qt::UserRole).toString(), item->data(2, Qt::UserRole + 1).toBool(), edit);
    };
    connect(m_tree, &QTreeWidget::itemClicked, this, [locate](QTreeWidgetItem* item) { locate(item, false); });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [locate](QTreeWidgetItem* item) { locate(item, true); });
}
void CalculationIssuesDock::setEntries(const QVector<CalculationDiagnostic>& entries, const QString& status) {
    m_tree->clear(); m_details->clear(); m_status->setText(status);
    for (const auto& entry : entries) {
        auto* item = new QTreeWidgetItem(m_tree, {entry.error ? "错误" : "提示", entry.title, entry.message});
        item->setData(0, Qt::UserRole, entry.objectId); item->setData(1, Qt::UserRole, entry.streamId);
        item->setData(2, Qt::UserRole, entry.componentId); item->setData(2, Qt::UserRole + 1, entry.gradeField);
        item->setToolTip(1, entry.objectId); item->setToolTip(2, entry.message);
        if (entry.streamId.isEmpty()) item->setToolTip(1, "全局问题：请先检查相关实测值或流程连接");
    }
}
}

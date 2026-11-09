#include "ui/FlowTemplateDock.h"
#include "ui/FlowTemplatePreview.h"
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>
namespace afs {
FlowTemplateDock::FlowTemplateDock(QWidget* parent) : QDockWidget("流程模板库", parent) {
    setObjectName("flowTemplateDock"); auto* body = new QWidget; auto* layout = new QVBoxLayout(body);
    auto* hint = new QLabel("选中设备和文字后保存为模板；所属药剂自动保留。单击模板预览，双击插入当前画布。"); hint->setWordWrap(true); layout->addWidget(hint);
    auto* search = new QLineEdit; search->setObjectName("templateSearch"); search->setPlaceholderText("搜索模板"); layout->addWidget(search);
    auto* splitter = new QSplitter(Qt::Vertical); splitter->setObjectName("templatePreviewSplitter");
    splitter->setChildrenCollapsible(false);
    m_list = new QListWidget; m_list->setObjectName("flowTemplateList"); m_list->setMinimumHeight(100);
    m_preview = new FlowTemplatePreview;
    splitter->addWidget(m_list); splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 1); splitter->setStretchFactor(1, 2); splitter->setSizes({180, 360});
    layout->addWidget(splitter, 1);
    auto* save = new QPushButton("保存选中流程为模板"); save->setObjectName("saveFlowTemplate"); layout->addWidget(save);
    auto* row = new QHBoxLayout; m_insert = new QPushButton("插入"); m_insert->setObjectName("insertFlowTemplate"); m_insert->setEnabled(false);
    auto* rename = new QPushButton("重命名"); auto* remove = new QPushButton("删除");
    row->addWidget(m_insert); row->addWidget(rename); row->addWidget(remove); layout->addLayout(row);
    auto* refresh = new QPushButton("刷新模板库"); layout->addWidget(refresh);
    m_status = new QLabel; m_status->setWordWrap(true); layout->addWidget(m_status); setWidget(body);
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        emit previewRequested(item ? item->data(Qt::UserRole).toString() : QString{});
    });
    connect(search, &QLineEdit::textChanged, this, [this](const QString& text) {
        for (int i = 0; i < m_list->count(); ++i) m_list->item(i)->setHidden(!m_list->item(i)->text().contains(text, Qt::CaseInsensitive));
        if (!m_list->currentItem() || m_list->currentItem()->isHidden()) {
            m_list->setCurrentRow(-1);
            for (int i = 0; i < m_list->count(); ++i) if (!m_list->item(i)->isHidden()) { m_list->setCurrentRow(i); break; }
        }
    });
    connect(save, &QPushButton::clicked, this, &FlowTemplateDock::saveSelectionRequested);
    connect(m_insert, &QPushButton::clicked, this, [this] { if (auto* item = m_list->currentItem()) emit insertRequested(item->data(Qt::UserRole).toString()); });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (m_insert->isEnabled()) emit insertRequested(item->data(Qt::UserRole).toString());
    });
    connect(rename, &QPushButton::clicked, this, [this] {
        auto* item = m_list->currentItem(); if (!item) return; bool accepted = false;
        const auto name = QInputDialog::getText(this, "重命名模板", "模板名称", QLineEdit::Normal, item->data(Qt::UserRole + 1).toString(), &accepted).trimmed();
        if (accepted && !name.isEmpty()) emit renameRequested(item->data(Qt::UserRole).toString(), name);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        auto* item = m_list->currentItem(); if (!item) return;
        if (QMessageBox::question(this, "删除模板", "删除该本机模板？当前项目中的设备将保留。") == QMessageBox::Yes) emit removeRequested(item->data(Qt::UserRole).toString());
    });
    connect(refresh, &QPushButton::clicked, this, &FlowTemplateDock::refreshRequested);
}
QString FlowTemplateDock::askTemplateName() {
    bool accepted = false;
    const auto name = QInputDialog::getText(window(), "保存流程模板", "模板名称", QLineEdit::Normal, {}, &accepted).trimmed();
    return accepted ? name : QString{};
}
void FlowTemplateDock::setTemplates(const QVector<FlowTemplateInfo>& templates, const QString& warning) {
    const auto selected = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QString{};
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const auto& entry : templates) {
        auto* item = new QListWidgetItem(QString("%1 · %2 个设备").arg(entry.name).arg(entry.units), m_list);
        item->setData(Qt::UserRole, entry.id); item->setData(Qt::UserRole + 1, entry.name);
        item->setHidden(!item->text().contains(findChild<QLineEdit*>("templateSearch")->text(), Qt::CaseInsensitive));
        if (entry.id == selected && !item->isHidden()) m_list->setCurrentItem(item);
    }
    if (!m_list->currentItem())
        for (int i = 0; i < m_list->count(); ++i) if (!m_list->item(i)->isHidden()) { m_list->setCurrentRow(i); break; }
    m_status->setText(warning.isEmpty() ? QString("本机模板：%1 个").arg(templates.size()) : warning);
    emit previewRequested(m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QString{});
}
void FlowTemplateDock::setPreview(QGraphicsScene* scene, const QString& message) {
    m_preview->setScene(scene, message);
    m_insert->setEnabled(scene != nullptr && m_list->currentItem() && !m_list->currentItem()->isHidden());
}
}

#include "ui/StreamOrderDialog.h"
#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace afs {
namespace {
QString tr(const char* text) { return QCoreApplication::translate("MainWindow", text); }
}

std::optional<QStringList> editStreamOrderDialog(
    QWidget* parent, const QVector<StreamOrderEntry>& ordered,
    const std::function<QVector<StreamOrderEntry>()>& flowOrder) {
    QDialog dialog(parent); dialog.setWindowTitle(tr("Excel 导出顺序")); dialog.resize(520, 520);
    auto* layout = new QVBoxLayout(&dialog);
    auto* hint = new QLabel(tr("仅列出已命名的产品。拖动调整 Excel 导出顺序。"), &dialog);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* list = new QListWidget(&dialog);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setSelectionMode(QAbstractItemView::SingleSelection); layout->addWidget(list, 1);
    const auto appendItem = [list](const StreamOrderEntry& stream) {
        auto* item = new QListWidgetItem(stream.displayName, list);
        item->setData(Qt::UserRole, stream.streamId);
        item->setToolTip(tr("物流 ID：%1").arg(stream.streamId));
    };
    for (const auto& stream : ordered) appendItem(stream);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto* reset = buttons->addButton(tr("恢复流程顺序"), QDialogButtonBox::ResetRole);
    QObject::connect(reset, &QPushButton::clicked, &dialog, [list, flowOrder, appendItem] {
        list->clear();
        for (const auto& stream : flowOrder()) appendItem(stream);
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    QStringList order;
    for (int row = 0; row < list->count(); ++row)
        order.append(list->item(row)->data(Qt::UserRole).toString());
    return order;
}

} // namespace afs

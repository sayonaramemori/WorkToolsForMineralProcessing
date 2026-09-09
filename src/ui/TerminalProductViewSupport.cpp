#include "ui/TerminalProductViewSupport.h"
#include "ui/TerminalProductTableModel.h"

#include <QDoubleValidator>
#include <QLineEdit>
#include <QPainter>
#include <QStyle>

namespace afs {

void StreamFilterProxyModel::setMode(Mode mode) {
    beginFilterChange(); m_mode = mode; endFilterChange(Direction::Rows);
}

void StreamFilterProxyModel::setInterestedOwners(QSet<QString> ids) {
    beginFilterChange(); m_interestedOwners = std::move(ids);
    endFilterChange(Direction::Rows);
}

bool StreamFilterProxyModel::filterAcceptsRow(int row, const QModelIndex& parent) const {
    const auto* model = qobject_cast<const TerminalProductTableModel*>(sourceModel());
    const auto* stream = model ? model->streamAt(row) : nullptr;
    if (!model || !stream) return false;
    if (!m_interestedOwners.isEmpty()) {
        bool belongs = false;
        for (const auto& ownerId : stream->ownerIds)
            if (m_interestedOwners.contains(ownerId)) { belongs = true; break; }
        if (!belongs) return false;
    }
    if (m_mode == All) return true;
    const QString status = model->index(row, model->statusColumn(), parent)
                               .data(Qt::DisplayRole).toString();
    if (m_mode == Entered) return status == QStringLiteral("实测值")
        || status == QStringLiteral("协调值") || status == QStringLiteral("填写中");
    if (m_mode == NeedsInput) return status == QStringLiteral("填写中")
        || status == QStringLiteral("可填写");
    if (m_mode == Terminal) return stream->terminal;
    if (m_mode == Feed) return stream->feed;
    return stream->recycle;
}

NumberDelegate::NumberDelegate(double maximum, QObject* parent)
    : QStyledItemDelegate(parent), m_maximum(maximum) {}

QWidget* NumberDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&,
                                      const QModelIndex&) const {
    auto* editor = new QLineEdit(parent);
    auto* validator = new QDoubleValidator(0.0, m_maximum, 6, editor);
    validator->setNotation(QDoubleValidator::StandardNotation);
    editor->setValidator(validator); editor->setAlignment(Qt::AlignCenter);
    editor->setFrame(false);
    return editor;
}

void NumberDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    auto* lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(index.data(Qt::EditRole).toString()); lineEdit->selectAll();
}

void NumberDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                  const QModelIndex& index) const {
    model->setData(index, static_cast<QLineEdit*>(editor)->text(), Qt::EditRole);
}

void StatusDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const {
    QStyleOptionViewItem base(option); initStyleOption(&base, index); base.text.clear();
    QStyledItemDelegate::paint(painter, base, index);
    const QString value = index.data(Qt::DisplayRole).toString();
    const bool ready = value == QStringLiteral("实测值")
        || value == QStringLiteral("计算值") || value == QStringLiteral("协调值");
    const QColor foreground = ready ? QColor("#17864b") : QColor("#a86400");
    QColor background = ready ? QColor("#dff5e8") : QColor("#fff0d6");
    if (option.state & QStyle::State_Selected) {
        background = option.palette.color(QPalette::HighlightedText); background.setAlpha(225);
    }
    const int width = QFontMetrics(option.font).horizontalAdvance(value) + 18;
    const QRect badge(option.rect.center().x() - width / 2,
                      option.rect.center().y() - 12, width, 24);
    painter->save(); painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen); painter->setBrush(background);
    painter->drawRoundedRect(badge, 12, 12); painter->setPen(foreground);
    painter->drawText(badge, Qt::AlignCenter, value); painter->restore();
}

} // namespace afs

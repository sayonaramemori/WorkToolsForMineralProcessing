#include "annotations/AnnotationItem.h"

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTextDocument>
#include <QPainter>
#include <QPalette>
#include <algorithm>
#include <cmath>
#include <utility>

namespace afs {
namespace {
QFont annotationFont(const AnnotationRecord& record,
                     const AnnotationTextSettings& settings) {
    QFont font = qApp->font();
    if (record.kind == AnnotationKind::UserNote && !record.noteFontFamily.isEmpty())
        font.setFamily(record.noteFontFamily);
    font.setPointSize(record.kind == AnnotationKind::UserNote && record.notePointSize > 0
        ? record.notePointSize : settings.pointSize);
    font.setBold(record.kind == AnnotationKind::UserNote && record.notePointSize > 0
        ? record.noteBold : settings.bold);
    return font;
}

QSizeF laidOutTextSize(const QString& text, const QFont& font) {
    QTextDocument document;
    document.setDocumentMargin(0);
    document.setDefaultFont(font);
    document.setPlainText(text);
    return document.size();
}
}

bool AnnotationItem::editUserNote(QWidget* parent, AnnotationRecord& record,
                                  const AnnotationTextSettings& defaults,
                                  const QString& title) {
    QDialog dialog(parent); dialog.setWindowTitle(title);
    auto* layout = new QFormLayout(&dialog);
    auto* text = new QPlainTextEdit(record.text, &dialog);
    text->setPlaceholderText(QObject::tr("输入自定义文字，可使用多行"));
    text->setMinimumSize(360, 130);
    auto* family = new QFontComboBox(&dialog);
    const QString initialFamily = record.noteFontFamily.isEmpty()
        ? QApplication::font().family() : record.noteFontFamily;
    family->setCurrentFont(QFont(initialFamily));
    auto* size = new QSpinBox(&dialog);
    size->setRange(7, 72);
    size->setValue(record.notePointSize > 0 ? record.notePointSize : defaults.pointSize);
    size->setSuffix(QObject::tr(" pt"));
    auto* bold = new QCheckBox(QObject::tr("使用粗体"), &dialog);
    bold->setChecked(record.notePointSize > 0 ? record.noteBold : defaults.bold);
    auto* border = new QCheckBox(QObject::tr("显示边框和背景"), &dialog);
    border->setChecked(record.noteBorderVisible);
    layout->addRow(QObject::tr("文字内容"), text);
    layout->addRow(QObject::tr("字体"), family);
    layout->addRow(QObject::tr("字号"), size);
    layout->addRow(QString(), bold);
    layout->addRow(QString(), border);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) return false;
    const QString content = text->toPlainText().trimmed().left(500);
    if (content.isEmpty()) return false;
    record.text = content;
    record.noteFontFamily = family->currentFont().family();
    record.notePointSize = size->value();
    record.noteBold = bold->isChecked();
    record.noteBorderVisible = border->isChecked();
    return true;
}

AnnotationItem::AnnotationItem(AnnotationRecord record, QString text)
    : m_record(std::move(record)), m_text(std::move(text)) {
    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setCursor(Qt::SizeAllCursor);
    setZValue(10.0);
    updateBounds();
}

void AnnotationItem::updateBounds() {
    prepareGeometryChange();
    const QSizeF content = laidOutTextSize(m_text, annotationFont(m_record, m_textSettings));
    m_bounds = QRectF(0, 0, std::ceil(content.width()) + 20.0,
                      std::ceil(content.height()) + 16.0);
}

QRectF AnnotationItem::boundingRect() const { return m_bounds.adjusted(-2, -2, 2, 2); }

void AnnotationItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    const QPalette palette = qApp->palette();
    QColor background = palette.color(QPalette::Base);
    background.setAlpha(230);
    const QColor border = isSelected() ? palette.color(QPalette::Highlight)
                                       : palette.color(QPalette::Midlight);
    painter->setRenderHint(QPainter::Antialiasing);
    if (m_record.kind != AnnotationKind::UserNote || m_record.noteBorderVisible) {
        painter->setPen(QPen(border, isSelected() ? 1.8 : 1.0));
        painter->setBrush(background);
        painter->drawRoundedRect(m_bounds, 6, 6);
    } else if (isSelected()) {
        painter->setPen(QPen(border, 1.0, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_bounds);
    }
    QTextDocument document;
    document.setDocumentMargin(0);
    document.setDefaultFont(annotationFont(m_record, m_textSettings));
    document.setPlainText(m_text);
    QAbstractTextDocumentLayout::PaintContext context;
    const QColor textColor = m_textSettings.color.isValid()
        ? m_textSettings.color : palette.color(QPalette::Text);
    context.palette.setColor(QPalette::Text, textColor);
    context.palette.setColor(QPalette::WindowText, textColor);
    painter->save();
    painter->translate(10.0, 8.0);
    document.documentLayout()->draw(painter, context);
    painter->restore();
}

void AnnotationItem::setText(QString text) {
    if (m_text == text) return;
    m_text = std::move(text);
    updateBounds();
    update();
}

void AnnotationItem::setTextSettings(AnnotationTextSettings settings) {
    m_textSettings = std::move(settings);
    updateBounds();
    update();
}

void AnnotationItem::setAnchor(const QPointF& sceneAnchor, const QPointF& defaultOffset) {
    m_anchor = sceneAnchor;
    if (!m_hasAnchor && !m_record.manuallyPlaced) m_record.manualOffset = defaultOffset;
    m_hasAnchor = true;
    m_updatingPosition = true;
    setPos(m_anchor + m_record.manualOffset);
    m_updatingPosition = false;
}

void AnnotationItem::refreshAppearance() { update(); }

QVariant AnnotationItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged && m_hasAnchor && !m_updatingPosition) {
        m_record.manualOffset = value.toPointF() - m_anchor;
        m_record.manuallyPlaced = true;
        emit placementEdited(m_record);
    }
    return QGraphicsObject::itemChange(change, value);
}

void AnnotationItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (m_record.kind != AnnotationKind::UserNote) {
        QGraphicsObject::mouseDoubleClickEvent(event);
        return;
    }
    auto edited = m_record;
    if (editUserNote(nullptr, edited, m_textSettings, tr("编辑文字标注"))) {
        m_record = std::move(edited);
        setText(m_record.text);
        emit recordEdited(m_record);
    }
    event->accept();
}

} // namespace afs

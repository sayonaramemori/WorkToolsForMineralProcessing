#include "annotations/ReagentAnnotationItem.h"

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QTextDocument>

namespace afs {
namespace { constexpr qreal kCrossRadius = 5.0; constexpr qreal kGap = 9.0; }

namespace {
QString escaped(const QString& text) { return text.toHtmlEscaped(); }

QString chemicalHtml(const QString& source) {
    QString result;
    for (qsizetype i = 0; i < source.size();) {
        const QChar current = source[i];
        if (current == '\n') { result += "<br>"; ++i; continue; }
        if (current == '^' || current == '_') {
            const bool superscript = current == '^';
            ++i;
            QString value;
            if (i < source.size() && source[i] == '{') {
                const qsizetype end = source.indexOf('}', i + 1);
                if (end >= 0) { value = source.mid(i + 1, end - i - 1); i = end + 1; }
                else { result += escaped(QString(current)); continue; }
            } else {
                if (i < source.size() && source[i].isDigit()) {
                    while (i < source.size()
                           && (source[i].isDigit() || source[i] == '+' || source[i] == '-'))
                        value += source[i++];
                } else if (i < source.size()) {
                    value += source[i++];
                }
            }
            if (!value.isEmpty())
                result += QString("<%1>%2</%1>").arg(superscript ? "sup" : "sub", escaped(value));
            continue;
        }
        result += escaped(QString(current));
        ++i;
    }
    return result;
}
}

QString ReagentAnnotationItem::formattedHtml() const {
    QString source = m_record.text;
    if (!m_record.dosage.isEmpty() || !m_record.dosageUnit.isEmpty())
        source += "\n" + QString("%1 %2").arg(m_record.dosage, m_record.dosageUnit).trimmed();
    if (!m_record.note.isEmpty()) source += "\n" + m_record.note;
    return chemicalHtml(source);
}

bool ReagentAnnotationItem::editRecord(QWidget* parent, AnnotationRecord& record,
                                       const QString& title) {
    QDialog dialog(parent); dialog.setWindowTitle(title);
    auto* layout = new QFormLayout(&dialog);
    auto* name = new QLineEdit(record.text, &dialog);
    name->setPlaceholderText(QObject::tr("例如 CuSO_4 或 3418A"));
    auto* dosage = new QLineEdit(record.dosage, &dialog);
    dosage->setPlaceholderText(QObject::tr("例如 120"));
    auto* unit = new QLineEdit(record.dosageUnit, &dialog);
    unit->setPlaceholderText(QObject::tr("例如 g/t、kg/t、mL"));
    auto* note = new QLineEdit(record.note, &dialog);
    layout->addRow(QObject::tr("药剂名称"), name);
    layout->addRow(QObject::tr("用量"), dosage);
    layout->addRow(QObject::tr("单位"), unit);
    layout->addRow(QObject::tr("备注"), note);
    auto* hint = new QLabel(QObject::tr("上下标：^ 表示上标，_ 表示下标；普通数字保持原样"), &dialog);
    hint->setWordWrap(true); layout->addRow(hint);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) return false;
    record.text = name->text().trimmed().left(120);
    record.dosage = dosage->text().trimmed().left(40);
    record.dosageUnit = unit->text().trimmed().left(24);
    record.note = note->text().trimmed().left(120);
    return !record.text.isEmpty() || !record.dosage.isEmpty() || !record.note.isEmpty();
}

ReagentAnnotationItem::ReagentAnnotationItem(AnnotationRecord record)
    : m_record(std::move(record)) {
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(11.0);
    updateBounds();
}

void ReagentAnnotationItem::setRecord(const AnnotationRecord& record) {
    m_record = record;
    updateBounds();
    update();
}

void ReagentAnnotationItem::updateBounds() {
    prepareGeometryChange();
    QFont font = qApp->font();
    font.setPointSize(m_textSettings.pointSize);
    font.setBold(m_textSettings.bold);
    QTextDocument document;
    document.setDocumentMargin(0);
    document.setDefaultFont(font);
    document.setHtml(formattedHtml());
    const QSizeF content = document.size();
    m_textBounds = QRectF(kCrossRadius + kGap, -content.height() / 2.0 - 5.0,
                         content.width() + 16.0, content.height() + 10.0);
}

QRectF ReagentAnnotationItem::boundingRect() const {
    return m_textBounds.united(QRectF(-kCrossRadius - 2, -kCrossRadius - 2,
                                     2 * kCrossRadius + 4, 2 * kCrossRadius + 4));
}

QPainterPath ReagentAnnotationItem::shape() const {
    QPainterPath path;
    path.addRect(m_textBounds.adjusted(-2, -2, 2, 2));
    path.addRect(QRectF(-8, -8, 16, 16));
    return path;
}

void ReagentAnnotationItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    const QPalette palette = qApp->palette();
    const QColor textColor = m_textSettings.color.isValid()
        ? m_textSettings.color : palette.color(QPalette::Text);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(textColor, 2.0));
    painter->drawLine(QPointF(-kCrossRadius, -kCrossRadius), QPointF(kCrossRadius, kCrossRadius));
    painter->drawLine(QPointF(-kCrossRadius, kCrossRadius), QPointF(kCrossRadius, -kCrossRadius));
    QColor background = palette.color(QPalette::Base); background.setAlpha(230);
    painter->setPen(QPen(isSelected() ? palette.color(QPalette::Highlight)
                                      : palette.color(QPalette::Midlight), isSelected() ? 1.8 : 1.0));
    painter->setBrush(background);
    painter->drawRoundedRect(m_textBounds, 4, 4);
    QFont font = qApp->font(); font.setPointSize(m_textSettings.pointSize); font.setBold(m_textSettings.bold);
    QTextDocument document;
    document.setDocumentMargin(0);
    document.setDefaultFont(font);
    document.setHtml(formattedHtml());
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, textColor);
    context.palette.setColor(QPalette::WindowText, textColor);
    painter->save();
    painter->translate(m_textBounds.left() + 8.0,
                       m_textBounds.center().y() - document.size().height() / 2.0);
    document.documentLayout()->draw(painter, context);
    painter->restore();
}

void ReagentAnnotationItem::setAnchor(const QPointF& anchor) {
    m_anchor = anchor;
    m_hasAnchor = true;
    m_updatingPosition = true;
    setPos(anchor + QPointF(0.0, m_record.manualOffset.y()));
    m_updatingPosition = false;
}

QVariant ReagentAnnotationItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && m_hasAnchor && !m_updatingPosition) {
        const QPointF requested = value.toPointF();
        return QPointF(m_anchor.x(), requested.y());
    }
    if (change == ItemPositionHasChanged && m_hasAnchor && !m_updatingPosition) {
        m_record.manualOffset = QPointF(0.0, value.toPointF().y() - m_anchor.y());
        m_record.manuallyPlaced = true;
        emit recordEdited(m_record);
    }
    return QGraphicsObject::itemChange(change, value);
}

void ReagentAnnotationItem::setTextSettings(const AnnotationTextSettings& settings) {
    m_textSettings = settings; updateBounds(); update();
}

void ReagentAnnotationItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (editRecord(nullptr, m_record, tr("编辑药剂标注"))) {
        updateBounds(); update(); emit recordEdited(m_record);
    }
    event->accept();
}

} // namespace afs

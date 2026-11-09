#include "graphics/items/FlotationUnitItem.h"
#include "graphics/symbols/UnitSymbolPainter.h"
#include <QApplication>
#include <QPainter>
#include <QPalette>

namespace afs {
void FlotationUnitItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    const QPalette palette = QApplication::palette();
    const QColor color = isSelected() ? palette.color(QPalette::Highlight)
                                      : palette.color(QPalette::Text);
    symbols::paintUnit(painter, m_unit, color, poolDisplayLabel());
}
}

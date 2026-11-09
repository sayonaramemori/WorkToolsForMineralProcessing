#pragma once
#include "core/FlotationUnit.h"
class QPainter;
class QColor;
namespace afs::symbols {
// Draw only the device body; item state, ports and interaction stay with the item.
void paintUnit(QPainter* painter, const FlotationUnit& unit, const QColor& color,
               const QString& poolLabel);
}

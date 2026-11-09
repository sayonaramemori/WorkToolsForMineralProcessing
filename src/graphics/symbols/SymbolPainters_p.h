#pragma once
#include "graphics/symbols/UnitSymbolPainter.h"

namespace afs::symbols {
void paintBallMill(QPainter* painter, const FlotationUnit& unit, const QColor& color);
void paintThickener(QPainter* painter, const FlotationUnit& unit, const QColor& color);
void paintBinarySplitter(QPainter* painter, const FlotationUnit& unit, const QColor& color);
void paintCylindricalTank(QPainter* painter, const FlotationUnit& unit, const QColor& color);
void paintStoragePool(QPainter* painter, const FlotationUnit& unit, const QColor& color, const QString& label);
void paintGravitySeparator(QPainter* painter, const FlotationUnit& unit, const QColor& color);
void paintSedimentationTank(QPainter* painter, const FlotationUnit& unit, const QColor& color);
void paintSizingSeparator(QPainter* painter, const FlotationUnit& unit, const QColor& color);
void paintMagneticSeparator(QPainter* painter, const FlotationUnit& unit, const QColor& color);
void paintFlotation(QPainter* painter, const FlotationUnit& unit, const QColor& color);
}

#include "graphics/symbols/SymbolPainters_p.h"

namespace afs::symbols {
void paintUnit(QPainter* painter, const FlotationUnit& unit, const QColor& color, const QString& poolLabel) {
    if (unit.kind == UnitKind::Thickener) {
        paintThickener(painter, unit, color);
        return;
    }
    if (unit.kind == UnitKind::BallMill) {
        paintBallMill(painter, unit, color);
        return;
    }
    if (unit.kind == UnitKind::BinarySplitter) {
        paintBinarySplitter(painter, unit, color);
        return;
    }
    if (isCylindricalTank(unit.kind)) {
        paintCylindricalTank(painter, unit, color);
        return;
    }
    if (isStoragePool(unit.kind)) {
        paintStoragePool(painter, unit, color, poolLabel);
        return;
    }
    if (unit.kind == UnitKind::SpiralChute || unit.kind == UnitKind::ShakingTable
        || unit.kind == UnitKind::DenseMediumCyclone) {
        paintGravitySeparator(painter, unit, color);
        return;
    }
    if (unit.kind == UnitKind::SedimentationTank) {
        paintSedimentationTank(painter, unit, color);
        return;
    }
    if (isSizingSeparation(unit.kind)) {
        paintSizingSeparator(painter, unit, color);
        return;
    }
    if (isMagneticSeparation(unit.kind)) {
        paintMagneticSeparator(painter, unit, color);
        return;
    }
    paintFlotation(painter, unit, color);
}
}

#include "graphics/symbols/SymbolPainters_p.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/ThickenerGeometry.h"
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

namespace afs::symbols {
void paintBallMill(QPainter* painter, const FlotationUnit& unit, const QColor& color) {
    const double radius = unit.ballMillDiameter / 2.0;
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(0, 0), radius, radius);
}

void paintThickener(QPainter* painter, const FlotationUnit& unit, const QColor& color) {
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(ThickenerGeometry::rim(unit.width));
    painter->drawPath(ThickenerGeometry::walls(unit.width));
    // Central shaft and rake identify the basin without adding a caption.
    painter->drawLine(QPointF(0, -22), QPointF(0, 26));
    painter->drawLine(QPointF(-unit.width * 0.28, 8), QPointF(0, 26));
    painter->drawLine(QPointF(0, 26), QPointF(unit.width * 0.28, 8));
}

void paintCylindricalTank(QPainter* painter, const FlotationUnit& unit, const QColor& color) {
    const double halfWidth = unit.width / 2.0;
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    constexpr double tankHalfWidth = 42.0;
    painter->drawLine(QPointF(-halfWidth, 0), QPointF(-tankHalfWidth, 0));
    painter->drawLine(QPointF(tankHalfWidth, 0), QPointF(halfWidth, 0));
    painter->drawEllipse(QRectF(-tankHalfWidth, -28.0, tankHalfWidth * 2.0, 15.0));
    painter->drawLine(QPointF(-tankHalfWidth, -20.5), QPointF(-tankHalfWidth, 28.0));
    painter->drawLine(QPointF(tankHalfWidth, -20.5), QPointF(tankHalfWidth, 28.0));
    painter->drawArc(QRectF(-tankHalfWidth, 20.5, tankHalfWidth * 2.0, 15.0), 180 * 16,
                     180 * 16);
    QPainterPath liquid;
    liquid.moveTo(-tankHalfWidth + 6.0, 11.0);
    for (double x = -tankHalfWidth + 6.0; x < tankHalfWidth - 6.0; x += 16.0)
        liquid.cubicTo(x + 4.0, 7.0, x + 12.0, 15.0, x + 16.0, 11.0);
    painter->drawPath(liquid);
    if (unit.kind == UnitKind::MixingTank) {
        // A motor, shaft and impeller distinguish a mixing tank from the
        // otherwise similar media-storage barrel.
        painter->drawRect(QRectF(-7.0, -41.0, 14.0, 8.0));
        painter->drawLine(QPointF(0.0, -33.0), QPointF(0.0, 19.0));
        painter->drawLine(QPointF(-17.0, 15.0), QPointF(17.0, 15.0));
        painter->drawLine(QPointF(-17.0, 15.0), QPointF(-10.0, 10.0));
        painter->drawLine(QPointF(17.0, 15.0), QPointF(10.0, 10.0));
    }

}

void paintStoragePool(QPainter* painter, const FlotationUnit& unit, const QColor& color, const QString& label) {
    const double halfWidth = unit.width / 2.0;
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    // Open tank: deliberately no top edge; the wavy waterline makes the
    // pool distinct from a process vessel while retaining the same inlet.
    painter->drawLine(QPointF(-halfWidth, 0), QPointF(-halfWidth, 46.0));
    painter->drawLine(QPointF(halfWidth, 0), QPointF(halfWidth, 46.0));
    painter->drawLine(QPointF(-halfWidth, 46.0), QPointF(halfWidth, 46.0));
    QPainterPath water;
    water.moveTo(-halfWidth + 10.0, 25.0);
    for (double x = -halfWidth + 10.0; x < halfWidth - 10.0; x += 20.0) {
        water.cubicTo(x + 5.0, 20.0, x + 15.0, 30.0, x + 20.0, 25.0);
    }
    painter->drawPath(water);
    QFont font = painter->font();
    font.setPointSizeF(8.0);
    painter->setFont(font);
    painter->drawText(QRectF(-halfWidth + 8.0, 3.0, unit.width - 16.0, 16.0),
                      Qt::AlignHCenter | Qt::AlignVCenter, label);
}
}

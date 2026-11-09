#include "graphics/symbols/SymbolPainters_p.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/ThickenerGeometry.h"
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

namespace afs::symbols {
void paintBinarySplitter(QPainter* painter, const FlotationUnit& unit, const QColor& color) {
    const double halfWidth = unit.width / 2.0;
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    const double radius = 18.0;
    QPainterPath diamond;
    diamond.moveTo(0, -radius);
    diamond.lineTo(radius, 0);
    diamond.lineTo(0, radius);
    diamond.lineTo(-radius, 0);
    diamond.closeSubpath();
    painter->drawPath(diamond);
    painter->drawLine(QPointF(-radius, 0), QPointF(-halfWidth, 0));
    painter->drawLine(QPointF(radius, 0), QPointF(halfWidth, 0));

}

void paintGravitySeparator(QPainter* painter, const FlotationUnit& unit, const QColor& color) {
    const double halfWidth = unit.width / 2.0;
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    constexpr double iconHalfWidth = 46.0;
    painter->drawLine(QPointF(-halfWidth, 0), QPointF(-iconHalfWidth, 0));
    painter->drawLine(QPointF(iconHalfWidth, 0), QPointF(halfWidth, 0));
    if (unit.kind == UnitKind::SpiralChute) {
        // Stepped elliptical turns depict the descending spiral trough.
        painter->drawEllipse(QRectF(-30.0, -27.0, 60.0, 13.0));
        painter->drawEllipse(QRectF(-25.0, -16.0, 50.0, 13.0));
        painter->drawEllipse(QRectF(-20.0, -5.0, 40.0, 13.0));
        painter->drawLine(QPointF(-30.0, -20.5), QPointF(-25.0, -9.5));
        painter->drawLine(QPointF(30.0, -20.5), QPointF(25.0, -9.5));
        painter->drawLine(QPointF(-25.0, -9.5), QPointF(-20.0, 1.5));
        painter->drawLine(QPointF(25.0, -9.5), QPointF(20.0, 1.5));
    } else if (unit.kind == UnitKind::ShakingTable) {
        // An inclined deck and its transverse riffles identify a shaking
        // table while the normal left/right rails retain the shared
        // two-product material-balance topology.
        QPolygonF deck;
        deck << QPointF(-40.0, -23.0) << QPointF(39.0, -12.0)
             << QPointF(31.0, 19.0) << QPointF(-48.0, 8.0);
        painter->drawPolygon(deck);
        for (int index = 0; index < 6; ++index) {
            const double x = -32.0 + index * 11.5;
            const double y = -14.0 + index * 1.6;
            painter->drawLine(QPointF(x, y), QPointF(x - 6.0, y + 18.0));
        }
        painter->drawLine(QPointF(-31.0, 14.0), QPointF(-40.0, 29.0));
        painter->drawLine(QPointF(26.0, 22.0), QPointF(35.0, 33.0));
        painter->drawLine(QPointF(-44.0, -28.0), QPointF(-44.0, -21.0));
        painter->drawEllipse(QPointF(-44.0, -19.0), 1.8, 1.8);
    } else {
        // Cylinder and conical underflow form the conventional dense
        // medium cyclone process-flow symbol.
        painter->drawEllipse(QRectF(-28.0, -28.0, 56.0, 20.0));
        painter->drawLine(QPointF(-28.0, -18.0), QPointF(-13.0, 20.0));
        painter->drawLine(QPointF(28.0, -18.0), QPointF(13.0, 20.0));
        painter->drawLine(QPointF(-13.0, 20.0), QPointF(0.0, 30.0));
        painter->drawLine(QPointF(13.0, 20.0), QPointF(0.0, 30.0));
        QPainterPath swirl;
        swirl.moveTo(-12.0, -13.0);
        swirl.cubicTo(12.0, -26.0, 18.0, -2.0, 1.0, 2.0);
        painter->drawPath(swirl);
    }

}

void paintSedimentationTank(QPainter* painter, const FlotationUnit& unit, const QColor& color) {
    const double halfWidth = unit.width / 2.0;
    // A quiet liquid surface and a funnel-shaped bottom identify a
    // settling box while keeping the standard two-product rails.
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    constexpr double boxHalfWidth = 48.0;
    painter->drawLine(QPointF(-halfWidth, 0), QPointF(-boxHalfWidth, 0));
    painter->drawLine(QPointF(boxHalfWidth, 0), QPointF(halfWidth, 0));
    painter->drawLine(QPointF(-boxHalfWidth, -25.0), QPointF(boxHalfWidth, -25.0));
    painter->drawLine(QPointF(-boxHalfWidth, -25.0), QPointF(-boxHalfWidth, 16.0));
    painter->drawLine(QPointF(boxHalfWidth, -25.0), QPointF(boxHalfWidth, 16.0));
    painter->drawLine(QPointF(-boxHalfWidth, 16.0), QPointF(-16.0, 32.0));
    painter->drawLine(QPointF(boxHalfWidth, 16.0), QPointF(16.0, 32.0));
    painter->drawLine(QPointF(-16.0, 32.0), QPointF(16.0, 32.0));
    QPainterPath water;
    water.moveTo(-boxHalfWidth + 6.0, -5.0);
    for (double x = -boxHalfWidth + 6.0; x < boxHalfWidth - 6.0; x += 16.0)
        water.cubicTo(x + 4.0, -8.0, x + 12.0, -2.0, x + 16.0, -5.0);
    painter->drawPath(water);
    painter->drawLine(QPointF(0.0, -33.0), QPointF(0.0, -25.0));

}

void paintSizingSeparator(QPainter* painter, const FlotationUnit& unit, const QColor& color) {
    const double halfWidth = unit.width / 2.0;
    // Both symbols retain the regular left/right product rails so existing
    // connection and balance behavior remains unchanged.  The internals
    // distinguish a screen deck from an inclined spiral classifier.
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    constexpr double iconHalfWidth = 50.0;
    painter->drawLine(QPointF(-halfWidth, 0), QPointF(-iconHalfWidth, 0));
    painter->drawLine(QPointF(iconHalfWidth, 0), QPointF(halfWidth, 0));
    const bool demedium = unit.kind == UnitKind::DemediumScreen
        || unit.kind == UnitKind::ThreeProductDemediumScreen;
    const bool threeProductScreen = unit.kind == UnitKind::ThreeProductScreening
        || unit.kind == UnitKind::ThreeProductDemediumScreen;
    if (unit.kind == UnitKind::Screening || demedium || threeProductScreen) {
        QPolygonF deck;
        deck << QPointF(-38.0, -19.0) << QPointF(39.0, -5.0)
             << QPointF(35.0, 11.0) << QPointF(-42.0, -3.0);
        painter->drawPolygon(deck);
        if (threeProductScreen) {
            QPolygonF lowerDeck;
            lowerDeck << QPointF(-36.0, -6.0) << QPointF(37.0, 8.0)
                      << QPointF(33.0, 22.0) << QPointF(-40.0, 8.0);
            painter->drawPolygon(lowerDeck);
        }
        for (int index = 0; index < 6; ++index) {
            const double x = -25.0 + index * 10.0;
            const double y = -10.5 + index * 1.8;
            painter->drawEllipse(QPointF(x, y), 1.6, 1.6);
        }
        painter->drawLine(QPointF(-28.0, 9.0), QPointF(-37.0, 24.0));
        painter->drawLine(QPointF(27.0, 19.0), QPointF(36.0, 28.0));
        if (demedium) {
            painter->drawLine(QPointF(-24.0, -27.0), QPointF(-24.0, -20.0));
            painter->drawLine(QPointF(0.0, -23.0), QPointF(0.0, -16.0));
            painter->drawLine(QPointF(24.0, -19.0), QPointF(24.0, -12.0));
            painter->drawEllipse(QPointF(-24.0, -16.0), 1.5, 1.5);
            painter->drawEllipse(QPointF(0.0, -12.0), 1.5, 1.5);
            painter->drawEllipse(QPointF(24.0, -8.0), 1.5, 1.5);
        }
    } else {
        QPolygonF trough;
        trough << QPointF(-39.0, -19.0) << QPointF(38.0, -19.0)
               << QPointF(28.0, 22.0) << QPointF(-30.0, 22.0);
        painter->drawPolygon(trough);
        painter->drawLine(QPointF(-27.0, -13.0), QPointF(22.0, 16.0));
        for (int index = 0; index < 4; ++index)
            painter->drawEllipse(QPointF(-19.0 + index * 13.0,
                                          -8.0 + index * 7.5), 4.0, 4.0);
    }

}

void paintMagneticSeparator(QPainter* painter, const FlotationUnit& unit, const QColor& color) {
    const double halfWidth = unit.width / 2.0;
    // The product rails preserve the existing one-in/two-out connection
    // geometry. The central symbol distinguishes weak (drum) and strong
    // (vertical high-intensity) magnetic separation on a compact PFD.
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    constexpr double housingHalfWidth = 48.0;
    painter->drawLine(QPointF(-halfWidth, 0), QPointF(-housingHalfWidth, 0));
    painter->drawLine(QPointF(housingHalfWidth, 0), QPointF(halfWidth, 0));
    const bool strong = unit.kind == UnitKind::StrongMagneticSeparation;
    painter->drawRect(QRectF(-housingHalfWidth, strong ? -28.0 : -24.0,
                             housingHalfWidth * 2.0, strong ? 52.0 : 44.0));
    if (strong) {
        // Upright ring and opposed magnetic poles suggest a strong /
        // high-intensity separator without introducing new routing ports.
        painter->drawEllipse(QRectF(-13.0, -23.0, 26.0, 42.0));
        painter->drawLine(QPointF(-31.0, -17.0), QPointF(-31.0, 13.0));
        painter->drawLine(QPointF(31.0, -17.0), QPointF(31.0, 13.0));
        painter->drawLine(QPointF(-38.0, -17.0), QPointF(-24.0, -17.0));
        painter->drawLine(QPointF(-38.0, 13.0), QPointF(-24.0, 13.0));
        painter->drawLine(QPointF(24.0, -17.0), QPointF(38.0, -17.0));
        painter->drawLine(QPointF(24.0, 13.0), QPointF(38.0, 13.0));
    } else {
        painter->drawEllipse(QPointF(-12.0, -2.0), 13.0, 13.0);
        QPainterPath magnet;
        magnet.moveTo(28.0, -15.0);
        magnet.lineTo(38.0, -15.0);
        magnet.lineTo(38.0, 8.0);
        magnet.cubicTo(38.0, 20.0, 18.0, 20.0, 18.0, 8.0);
        magnet.lineTo(18.0, 0.0);
        painter->drawPath(magnet);
        painter->drawLine(QPointF(18.0, 0.0), QPointF(28.0, 0.0));
    }

}

void paintFlotation(QPainter* painter, const FlotationUnit& unit, const QColor& color) {
    const double halfWidth = unit.width / 2.0;
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(-halfWidth, 0), QPointF(-halfWidth, FlotationGeometry::DoubleLineGap));
    painter->drawLine(QPointF(halfWidth, 0), QPointF(halfWidth, FlotationGeometry::DoubleLineGap));
    painter->drawLine(QPointF(-halfWidth, FlotationGeometry::DoubleLineGap),
                      QPointF(halfWidth, FlotationGeometry::DoubleLineGap));

    // 参考论文图：槽体最上方横线更粗，并与两侧竖线闭合。
    painter->setPen(QPen(color, FlotationGeometry::TopLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->drawLine(QPointF(-halfWidth, 0), QPointF(halfWidth, 0));
}
}

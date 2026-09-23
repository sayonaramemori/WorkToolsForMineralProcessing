#include "graphics/items/FlotationUnitItem.h"
#include "graphics/FlotationGeometry.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPalette>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <utility>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kMinimumWidth = 160.0;
constexpr double kMaximumWidth = 720.0;

QString defaultPoolLabel(afs::UnitKind kind) {
    switch (kind) {
    case afs::UnitKind::TailingsPool: return QStringLiteral("尾矿池");
    case afs::UnitKind::ConcentratePool: return QStringLiteral("精矿池");
    case afs::UnitKind::WaterPool: return QStringLiteral("回水池");
    case afs::UnitKind::MediaTank: return QStringLiteral("介质桶");
    case afs::UnitKind::MixingTank: return QStringLiteral("混料桶");
    default: return {};
    }
}
}

namespace afs {

FlotationUnitItem::FlotationUnitItem(FlotationUnit unit) : m_unit(std::move(unit)) {
    setPos(m_unit.position);
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setCursor(Qt::OpenHandCursor);
    m_inputLine = new InputLineItem(this);
    if (isStoragePool(m_unit.kind)) {
        if (!isTerminalStoragePool(m_unit.kind))
            m_rightProduct = new ProductLineItem(this, ProductSide::Right);
    } else {
        m_leftProduct = new ProductLineItem(this, ProductSide::Left);
        if (hasMiddleProduct(m_unit.kind))
            m_middleProduct = new ProductLineItem(this, ProductSide::Middle);
        m_rightProduct = new ProductLineItem(this, ProductSide::Right);
    }
}

bool FlotationUnitItem::adjustWidth(double delta) {
    const double newWidth = std::clamp(m_unit.width + delta, kMinimumWidth, kMaximumWidth);
    if (qFuzzyCompare(newWidth, m_unit.width)) return false;
    const double oldWidth = m_unit.width;
    prepareGeometryChange();
    m_unit.width = newWidth;
    update();
    for (auto* product : products()) product->updatePath();
    if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
        flowsheet->sourceWidthChanged(this, oldWidth);
    return true;
}

bool FlotationUnitItem::setLeftSplitPercent(double percent) {
    if (m_unit.kind != UnitKind::BinarySplitter || !std::isfinite(percent)
        || percent <= 0.0 || percent >= 100.0 || qFuzzyCompare(percent, m_unit.leftSplitPercent))
        return false;
    m_unit.leftSplitPercent = percent;
    update();
    if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
        flowsheet->notifyTopologyChanged();
    return true;
}

QList<ProductLineItem*> FlotationUnitItem::products() const {
    QList<ProductLineItem*> result;
    if (m_leftProduct) result.append(m_leftProduct);
    if (m_middleProduct) result.append(m_middleProduct);
    if (m_rightProduct) result.append(m_rightProduct);
    return result;
}

QRectF FlotationUnitItem::boundingRect() const {
    const double halfWidth = m_unit.width / 2.0;
    if (m_unit.kind == UnitKind::BinarySplitter)
        return {-halfWidth - 4.0, -22.0, m_unit.width + 8.0, 86.0};
    if (isCylindricalTank(m_unit.kind))
        return {-halfWidth - 4.0, -34.0, m_unit.width + 8.0, 84.0};
    if (isStoragePool(m_unit.kind))
        return {-halfWidth - 4.0, -4.0, m_unit.width + 8.0, 58.0};
    if (m_unit.kind == UnitKind::SpiralChute || m_unit.kind == UnitKind::ShakingTable
        || m_unit.kind == UnitKind::DenseMediumCyclone)
        return {-halfWidth - 4.0, -32.0, m_unit.width + 8.0, 72.0};
    if (m_unit.kind == UnitKind::SedimentationTank)
        return {-halfWidth - 4.0, -34.0, m_unit.width + 8.0, 84.0};
    if (isSizingSeparation(m_unit.kind))
        return {-halfWidth - 4.0, -30.0, m_unit.width + 8.0, 76.0};
    if (isMagneticSeparation(m_unit.kind))
        return {-halfWidth - 4.0, -28.0, m_unit.width + 8.0, 62.0};
    return {-halfWidth - 4.0, -4.0, m_unit.width + 8.0,
            FlotationGeometry::DoubleLineGap + 8.0};
}

QPainterPath FlotationUnitItem::shape() const {
    const double halfWidth = m_unit.width / 2.0;
    QPainterPath body;
    if (m_unit.kind == UnitKind::BinarySplitter) {
        body.addRect(QRectF(-halfWidth, -20.0, m_unit.width, 42.0));
        return body;
    }
    if (isCylindricalTank(m_unit.kind)) {
        body.addRect(QRectF(-halfWidth, -30.0, m_unit.width, 76.0));
        return body;
    }
    if (isStoragePool(m_unit.kind)) {
        body.addRect(QRectF(-halfWidth, 0, m_unit.width, 50.0));
        return body;
    }
    if (m_unit.kind == UnitKind::SpiralChute || m_unit.kind == UnitKind::ShakingTable
        || m_unit.kind == UnitKind::DenseMediumCyclone) {
        body.addRect(QRectF(-halfWidth, -30.0, m_unit.width, 64.0));
        return body;
    }
    if (m_unit.kind == UnitKind::SedimentationTank) {
        body.addRect(QRectF(-halfWidth, -30.0, m_unit.width, 72.0));
        return body;
    }
    if (isSizingSeparation(m_unit.kind)) {
        body.addRect(QRectF(-halfWidth, -28.0, m_unit.width, 68.0));
        return body;
    }
    if (isMagneticSeparation(m_unit.kind)) {
        body.addRect(QRectF(-halfWidth, -26.0, m_unit.width, 54.0));
        return body;
    }
    body.addRect(QRectF(-halfWidth, 0, m_unit.width, FlotationGeometry::DoubleLineGap));
    return body;
}

void FlotationUnitItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    const QPalette palette = QApplication::palette();
    const QColor color = isSelected() ? palette.color(QPalette::Highlight)
                                      : palette.color(QPalette::Text);
    const double halfWidth = m_unit.width / 2.0;
    if (m_unit.kind == UnitKind::BinarySplitter) {
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
        painter->drawText(QRectF(-90, radius + 4, 180, 42), Qt::AlignHCenter | Qt::AlignTop,
                          QStringLiteral("二分流  左 %1% / 右 %2%")
                              .arg(m_unit.leftSplitPercent, 0, 'f', 1)
                              .arg(100.0 - m_unit.leftSplitPercent, 0, 'f', 1));
        return;
    }
    if (isCylindricalTank(m_unit.kind)) {
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
        if (m_unit.kind == UnitKind::MixingTank) {
            // A motor, shaft and impeller distinguish a mixing tank from the
            // otherwise similar media-storage barrel.
            painter->drawRect(QRectF(-7.0, -41.0, 14.0, 8.0));
            painter->drawLine(QPointF(0.0, -33.0), QPointF(0.0, 19.0));
            painter->drawLine(QPointF(-17.0, 15.0), QPointF(17.0, 15.0));
            painter->drawLine(QPointF(-17.0, 15.0), QPointF(-10.0, 10.0));
            painter->drawLine(QPointF(17.0, 15.0), QPointF(10.0, 10.0));
        }
        QFont font = painter->font();
        font.setPointSizeF(7.0);
        painter->setFont(font);
        const QString label = m_unit.poolLabel.isEmpty()
            ? defaultPoolLabel(m_unit.kind) : m_unit.poolLabel;
        painter->drawText(QRectF(-tankHalfWidth - 20.0, 38.0, tankHalfWidth * 2.0 + 40.0, 14.0),
                          Qt::AlignHCenter | Qt::AlignTop, label);
        return;
    }
    if (isStoragePool(m_unit.kind)) {
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
        const QString label = m_unit.poolLabel.isEmpty()
            ? defaultPoolLabel(m_unit.kind) : m_unit.poolLabel;
        QFont font = painter->font();
        font.setPointSizeF(8.0);
        painter->setFont(font);
        painter->drawText(QRectF(-halfWidth + 8.0, 3.0, m_unit.width - 16.0, 16.0),
                          Qt::AlignHCenter | Qt::AlignVCenter, label);
        return;
    }
    if (m_unit.kind == UnitKind::SpiralChute || m_unit.kind == UnitKind::ShakingTable
        || m_unit.kind == UnitKind::DenseMediumCyclone) {
        painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                             Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        constexpr double iconHalfWidth = 46.0;
        painter->drawLine(QPointF(-halfWidth, 0), QPointF(-iconHalfWidth, 0));
        painter->drawLine(QPointF(iconHalfWidth, 0), QPointF(halfWidth, 0));
        if (m_unit.kind == UnitKind::SpiralChute) {
            // Stepped elliptical turns depict the descending spiral trough.
            painter->drawEllipse(QRectF(-30.0, -27.0, 60.0, 13.0));
            painter->drawEllipse(QRectF(-25.0, -16.0, 50.0, 13.0));
            painter->drawEllipse(QRectF(-20.0, -5.0, 40.0, 13.0));
            painter->drawLine(QPointF(-30.0, -20.5), QPointF(-25.0, -9.5));
            painter->drawLine(QPointF(30.0, -20.5), QPointF(25.0, -9.5));
            painter->drawLine(QPointF(-25.0, -9.5), QPointF(-20.0, 1.5));
            painter->drawLine(QPointF(25.0, -9.5), QPointF(20.0, 1.5));
        } else if (m_unit.kind == UnitKind::ShakingTable) {
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
        QFont font = painter->font();
        font.setPointSizeF(7.0);
        painter->setFont(font);
        painter->drawText(QRectF(-iconHalfWidth, 34.0, iconHalfWidth * 2.0, 14.0),
                          Qt::AlignHCenter | Qt::AlignTop,
                          m_unit.kind == UnitKind::SpiralChute
                              ? QStringLiteral("螺旋溜槽")
                              : m_unit.kind == UnitKind::ShakingTable
                                  ? QStringLiteral("摇床") : QStringLiteral("重介质旋流器"));
        return;
    }
    if (m_unit.kind == UnitKind::SedimentationTank) {
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
        QFont font = painter->font();
        font.setPointSizeF(7.0);
        painter->setFont(font);
        painter->drawText(QRectF(-boxHalfWidth, 36.0, boxHalfWidth * 2.0, 14.0),
                          Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("沉降箱"));
        return;
    }
    if (isSizingSeparation(m_unit.kind)) {
        // Both symbols retain the regular left/right product rails so existing
        // connection and balance behavior remains unchanged.  The internals
        // distinguish a screen deck from an inclined spiral classifier.
        painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                             Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        constexpr double iconHalfWidth = 50.0;
        painter->drawLine(QPointF(-halfWidth, 0), QPointF(-iconHalfWidth, 0));
        painter->drawLine(QPointF(iconHalfWidth, 0), QPointF(halfWidth, 0));
        const bool demedium = m_unit.kind == UnitKind::DemediumScreen
            || m_unit.kind == UnitKind::ThreeProductDemediumScreen;
        const bool threeProductScreen = m_unit.kind == UnitKind::ThreeProductScreening
            || m_unit.kind == UnitKind::ThreeProductDemediumScreen;
        if (m_unit.kind == UnitKind::Screening || demedium || threeProductScreen) {
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
        QFont font = painter->font();
        font.setPointSizeF(7.0);
        painter->setFont(font);
        painter->drawText(QRectF(-iconHalfWidth, 30.0, iconHalfWidth * 2.0, 14.0),
                          Qt::AlignHCenter | Qt::AlignTop,
                          m_unit.kind == UnitKind::Classification
                              ? QStringLiteral("分级")
                              : m_unit.kind == UnitKind::DemediumScreen
                                  ? QStringLiteral("脱介筛")
                              : m_unit.kind == UnitKind::ThreeProductScreening
                                  ? QStringLiteral("三产品筛分")
                              : m_unit.kind == UnitKind::ThreeProductDemediumScreen
                                  ? QStringLiteral("三产品脱介筛") : QStringLiteral("筛分"));
        return;
    }
    if (isMagneticSeparation(m_unit.kind)) {
        // The product rails preserve the existing one-in/two-out connection
        // geometry. The central symbol distinguishes weak (drum) and strong
        // (vertical high-intensity) magnetic separation on a compact PFD.
        painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                             Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        constexpr double housingHalfWidth = 48.0;
        painter->drawLine(QPointF(-halfWidth, 0), QPointF(-housingHalfWidth, 0));
        painter->drawLine(QPointF(housingHalfWidth, 0), QPointF(halfWidth, 0));
        const bool strong = m_unit.kind == UnitKind::StrongMagneticSeparation;
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
        if (m_unit.kind != UnitKind::MagneticSeparation) {
            QFont font = painter->font();
            font.setPointSizeF(7.0);
            painter->setFont(font);
            painter->drawText(QRectF(-housingHalfWidth, 27.0, housingHalfWidth * 2.0, 14.0),
                              Qt::AlignHCenter | Qt::AlignTop,
                              strong ? QStringLiteral("强磁") : QStringLiteral("弱磁"));
        }
        return;
    }
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

void FlotationUnitItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (isStoragePool(m_unit.kind)) {
        bool accepted = false;
        const QString currentLabel = m_unit.poolLabel.isEmpty()
            ? defaultPoolLabel(m_unit.kind) : m_unit.poolLabel;
        const QString label = QInputDialog::getText(nullptr, QStringLiteral("设置贮池名称"),
            QStringLiteral("波浪线上方名称（留空恢复默认名称，最多 48 个字符）"),
            QLineEdit::Normal, currentLabel, &accepted).simplified().left(48);
        if (accepted && label != currentLabel) {
            m_unit.poolLabel = label;
            update();
            if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
                flowsheet->notifyRouteChanged();
        }
        event->accept();
        return;
    }
    if (m_unit.kind != UnitKind::BinarySplitter) {
        QGraphicsItem::mouseDoubleClickEvent(event);
        return;
    }
    bool accepted = false;
    const double value = QInputDialog::getDouble(nullptr, QStringLiteral("设置二分流比例"),
        QStringLiteral("左支路比例（右支路自动补足至 100%）"),
        m_unit.leftSplitPercent, 0.1, 99.9, 1, &accepted);
    if (accepted) setLeftSplitPercent(value);
    event->accept();
}

QVariant FlotationUnitItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && scene()) {
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
            flowsheet->moveConnectedPeers(this, value.toPointF() - pos());
    }
    if (change == ItemPositionHasChanged)
        m_unit.position = value.toPointF();
    if (change == ItemSelectedHasChanged)
        setCursor(value.toBool() ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    const QVariant result = QGraphicsItem::itemChange(change, value);
    if (change == ItemPositionHasChanged) {
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
            flowsheet->refreshConnections();
    }
    return result;
}

} // namespace afs

#include "graphics/items/FlotationUnitItem.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/ThickenerGeometry.h"
#include "graphics/CanvasInteraction.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QPainterPath>
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
    if (isNonSeparatingUnit(m_unit.kind)) {
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
    const bool ballMill = m_unit.kind == UnitKind::BallMill;
    const double newWidth = std::clamp(symbolWidth() + delta,
                                      ballMill ? 40.0 : kMinimumWidth, kMaximumWidth);
    if (qFuzzyCompare(newWidth, symbolWidth())) return false;
    const double oldWidth = m_unit.width;
    prepareGeometryChange();
    if (ballMill) m_unit.ballMillDiameter = newWidth;
    else m_unit.width = newWidth;
    m_inputLine->updateGeometry();
    update();
    for (auto* product : products()) product->updatePath();
    if (auto* host = canvasGeometryHost(scene()))
        host->sourceWidthChanged(this, oldWidth);
    return true;
}

bool FlotationUnitItem::adjustInputLength(double delta) {
    const double current = m_inputLine->length();
    const double requested = std::clamp(current + delta, 30.0, 5000.0);
    if (qFuzzyCompare(current, requested)) return false;
    m_unit.inputLength = requested;
    m_inputLine->updateGeometry();
    if (auto* host = canvasGeometryHost(scene())) host->refreshConnections();
    return true;
}

bool FlotationUnitItem::setLeftSplitPercent(double percent) {
    if (m_unit.kind != UnitKind::BinarySplitter || !std::isfinite(percent)
        || percent <= 0.0 || percent >= 100.0 || qFuzzyCompare(percent, m_unit.leftSplitPercent))
        return false;
    m_unit.leftSplitPercent = percent;
    update();
    if (auto* host = canvasGeometryHost(scene()))
        host->notifyTopologyChanged();
    return true;
}

QList<ProductLineItem*> FlotationUnitItem::products() const {
    QList<ProductLineItem*> result;
    if (m_leftProduct) result.append(m_leftProduct);
    if (m_middleProduct) result.append(m_middleProduct);
    if (m_rightProduct) result.append(m_rightProduct);
    return result;
}

QPointF FlotationUnitItem::inputAnchorScenePosition() const {
    return m_inputLine->mapToScene(m_inputLine->line().p2());
}

QRectF FlotationUnitItem::boundingRect() const {
    const double halfWidth = m_unit.width / 2.0;
    if (m_unit.kind == UnitKind::Thickener)
        return ThickenerGeometry::shape(m_unit.width).boundingRect().adjusted(-4, -4, 4, 4);
    if (m_unit.kind == UnitKind::BallMill) {
        const double extent = m_unit.ballMillDiameter / 2.0 + 4.0;
        return {-extent, -extent, extent * 2.0, extent * 2.0};
    }
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
    if (m_unit.kind == UnitKind::Thickener) return ThickenerGeometry::shape(m_unit.width);
    if (m_unit.kind == UnitKind::BallMill) {
        const double radius = m_unit.ballMillDiameter / 2.0;
        body.addEllipse(QPointF(0, 0), radius, radius);
        return body;
    }
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

QString FlotationUnitItem::poolDisplayLabel() const {
    return m_unit.poolLabel.isEmpty() ? defaultPoolLabel(m_unit.kind) : m_unit.poolLabel;
}

void FlotationUnitItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (isStoragePool(m_unit.kind)) {
        bool accepted = false;
        const QString currentLabel = poolDisplayLabel();
        const QString label = QInputDialog::getText(nullptr, QStringLiteral("设置贮池名称"),
            QStringLiteral("波浪线上方名称（留空恢复默认名称，最多 48 个字符）"),
            QLineEdit::Normal, currentLabel, &accepted).simplified().left(48);
        if (accepted && label != currentLabel) {
            m_unit.poolLabel = label;
            update();
            if (auto* host = canvasGeometryHost(scene()))
                host->notifyRouteChanged();
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
        if (auto* host = canvasGeometryHost(scene()))
            host->moveConnectedPeers(this, value.toPointF() - pos());
    }
    if (change == ItemPositionHasChanged)
        m_unit.position = value.toPointF();
    if (change == ItemSelectedHasChanged)
        setCursor(value.toBool() ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    const QVariant result = QGraphicsItem::itemChange(change, value);
    if (change == ItemPositionHasChanged) {
        if (auto* host = canvasGeometryHost(scene()))
            host->refreshConnections();
    }
    return result;
}

} // namespace afs

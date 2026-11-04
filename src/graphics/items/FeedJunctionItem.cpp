#include "graphics/items/FeedJunctionItem.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include <QApplication>
#include <QGraphicsScene>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <utility>

namespace afs {

FeedJunctionItem::FeedJunctionItem(QString id, ProductLineItem* recycleProduct,
                                   FlotationUnitItem* targetUnit,
                                   ProductLineItem* processProduct,
                                   MergeJunctionItem* processMerge)
    : m_id(std::move(id)), m_recycleProducts{recycleProduct},
      m_processProduct(processProduct), m_processMerge(processMerge), m_targetUnit(targetUnit) {
    setFlag(ItemIsSelectable);
    setFlag(ItemIsFocusable);
    setZValue(2.5);
    updatePath();
}

FeedJunctionItem::FeedJunctionItem(QString id, MergeJunctionItem* recycleMerge,
                                   FlotationUnitItem* targetUnit,
                                   ProductLineItem* processProduct,
                                   MergeJunctionItem* processMerge)
    : m_id(std::move(id)), m_recycleMerges{recycleMerge},
      m_processProduct(processProduct), m_processMerge(processMerge), m_targetUnit(targetUnit) {
    setFlag(ItemIsSelectable);
    setFlag(ItemIsFocusable);
    setZValue(2.5);
    updatePath();
}

QString FeedJunctionItem::recycleStreamId() const {
    if (auto* product = recycleProduct()) return product->streamId();
    if (auto* merge = recycleMerge()) return merge->outputStreamId();
    return {};
}

bool FeedJunctionItem::addRecycleProduct(ProductLineItem* product) {
    if (!product || m_recycleProducts.contains(product)) return false;
    m_recycleProducts.append(product);
    updatePath();
    return true;
}

bool FeedJunctionItem::addRecycleMerge(MergeJunctionItem* merge) {
    if (!merge || m_recycleMerges.contains(merge)) return false;
    m_recycleMerges.append(merge);
    updatePath();
    return true;
}

bool FeedJunctionItem::removeRecycleProduct(ProductLineItem* product) {
    if (!product || !m_recycleProducts.removeOne(product)) return false;
    const QString streamId = product->streamId();
    m_manualRouteXs.remove(streamId);
    m_manualRouteYs.remove(streamId);
    if (m_selectedStreamId == streamId) m_selectedStreamId.clear();
    if (m_editingStreamId == streamId) {
        m_editingStreamId.clear();
        m_editingAxis = EditingAxis::None;
    }
    updatePath();
    return true;
}

bool FeedJunctionItem::removeRecycleMerge(MergeJunctionItem* merge) {
    if (!merge || !m_recycleMerges.removeOne(merge)) return false;
    const QString streamId = merge->outputStreamId();
    m_manualRouteXs.remove(streamId);
    m_manualRouteYs.remove(streamId);
    if (m_selectedStreamId == streamId) m_selectedStreamId.clear();
    if (m_editingStreamId == streamId) {
        m_editingStreamId.clear();
        m_editingAxis = EditingAxis::None;
    }
    updatePath();
    return true;
}

bool FeedJunctionItem::selectSourceStream(const QString& streamId) {
    if (!m_sourcePaths.contains(streamId)) return false;
    m_selectedStreamId = streamId;
    setSelected(true);
    setFocus();
    update();
    return true;
}

void FeedJunctionItem::clearProcessSource() {
    m_processProduct = nullptr;
    m_processMerge = nullptr;
    m_hasExternalFeed = true;
    updatePath();
}

void FeedJunctionItem::setExternalFeed(bool enabled) {
    if (m_hasExternalFeed == enabled) return;
    m_hasExternalFeed = enabled;
    updatePath();
}

void FeedJunctionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    const QPalette palette = QApplication::palette();
    bool anotherFeedSelected = false;
    if (!isSelected() && scene()) {
        for (auto* item : scene()->selectedItems()) {
            if (dynamic_cast<FeedJunctionItem*>(item)) {
                anotherFeedSelected = true;
                break;
            }
        }
    }
    QColor color = isSelected() ? palette.color(QPalette::Highlight)
                                : palette.color(QPalette::Text);
    if (anotherFeedSelected) color.setAlphaF(0.22);
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_commonPath);
    for (auto it = m_sourcePaths.cbegin(); it != m_sourcePaths.cend(); ++it) {
        QColor branchColor = color;
        double width = FlotationGeometry::BodyLineWidth;
        if (isSelected() && !m_selectedStreamId.isEmpty()) {
            if (it.key() == m_selectedStreamId) width = 3.2;
            else branchColor.setAlphaF(0.3);
        } else if (isSelected()) {
            width = 2.6;
        }
        painter->setPen(QPen(branchColor, width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->drawPath(it.value());
        painter->setPen(Qt::NoPen);
        painter->setBrush(branchColor);
        painter->drawPath(m_sourceArrowPaths.value(it.key()));
        painter->setBrush(Qt::NoBrush);
    }
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPath(m_arrowPath);
    painter->drawEllipse(m_junctionPosition, JunctionRadius, JunctionRadius);
    if (isSelected()) {
        painter->setPen(QPen(palette.color(QPalette::Highlight), 1.5));
        painter->setBrush(palette.color(QPalette::Base));
        for (auto it = m_verticalHandles.cbegin(); it != m_verticalHandles.cend(); ++it) {
            painter->drawRect(QRectF(it.value() - QPointF(4, 4), QSizeF(8, 8)));
            const QPointF horizontal = m_horizontalHandles.value(it.key());
            painter->drawRect(QRectF(horizontal - QPointF(4, 4), QSizeF(8, 8)));
        }
    }
}

void FeedJunctionItem::refreshAppearance() {
    update();
}

QVariant FeedJunctionItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    const QVariant result = QGraphicsPathItem::itemChange(change, value);
    if (change == ItemSelectedHasChanged) {
        if (!value.toBool()) m_selectedStreamId.clear();
        refreshAppearance();
        if (scene())
            for (auto* item : scene()->items())
                if (auto* feed = dynamic_cast<FeedJunctionItem*>(item); feed && feed != this)
                    feed->refreshAppearance();
    }
    return result;
}

} // namespace afs

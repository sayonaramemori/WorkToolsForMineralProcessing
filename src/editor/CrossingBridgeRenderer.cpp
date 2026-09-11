#include "editor/CrossingBridgeRenderer.h"

#include "graphics/FlotationGeometry.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QApplication>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QLineF>
#include <QPainter>
#include <QPalette>
#include <cmath>

namespace afs {
namespace {
struct Segment { QGraphicsItem* owner; QPointF a; QPointF b; bool horizontal; };
}

void CrossingBridgeRenderer::draw(const QGraphicsScene& scene, QPainter& painter) {
    QVector<Segment> segments;
    for (auto* item : scene.items()) {
        auto* pathItem = dynamic_cast<QGraphicsPathItem*>(item);
        if (!pathItem || !pathItem->isVisible()
            || (!dynamic_cast<ProductLineItem*>(item)
                && !dynamic_cast<MergeJunctionItem*>(item)
                && !dynamic_cast<FeedJunctionItem*>(item))) continue;
        for (const auto& polygon : pathItem->path().toSubpathPolygons()) {
            for (int index = 1; index < polygon.size(); ++index) {
                const QPointF a = pathItem->mapToScene(polygon[index - 1]);
                const QPointF b = pathItem->mapToScene(polygon[index]);
                const bool horizontal = std::abs(a.y() - b.y()) < 0.01;
                const bool vertical = std::abs(a.x() - b.x()) < 0.01;
                if ((horizontal || vertical) && QLineF(a, b).length() > 12.0)
                    segments.append({item, a, b, horizontal});
            }
        }
    }
    constexpr double radius = 8.0;
    QVector<QPointF> crossings;
    for (int first = 0; first < segments.size(); ++first) {
        if (!segments[first].horizontal) continue;
        const auto& h = segments[first];
        const double left = std::min(h.a.x(), h.b.x());
        const double right = std::max(h.a.x(), h.b.x());
        for (int second = 0; second < segments.size(); ++second) {
            const auto& v = segments[second];
            if (v.horizontal || v.owner == h.owner) continue;
            const double top = std::min(v.a.y(), v.b.y());
            const double bottom = std::max(v.a.y(), v.b.y());
            const QPointF point(v.a.x(), h.a.y());
            if (point.x() <= left + radius || point.x() >= right - radius
                || point.y() <= top + 2.0 || point.y() >= bottom - 2.0) continue;
            bool duplicate = false;
            for (const auto& existing : crossings)
                duplicate = duplicate || QLineF(existing, point).length() < radius;
            if (!duplicate) crossings.append(point);
        }
    }
    const QPalette palette = QApplication::palette();
    const QColor background = scene.backgroundBrush().style() == Qt::NoBrush
        ? palette.color(QPalette::Base) : scene.backgroundBrush().color();
    painter.setRenderHint(QPainter::Antialiasing);
    for (const auto& point : crossings) {
        painter.setPen(QPen(background, 5.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(point - QPointF(radius + 1.0, 0), point + QPointF(radius + 1.0, 0));
        painter.setPen(QPen(palette.color(QPalette::Text), FlotationGeometry::BodyLineWidth,
                            Qt::SolidLine, Qt::RoundCap));
        QPainterPath bridge(point - QPointF(radius, 0));
        bridge.cubicTo(point + QPointF(-radius * 0.55, -radius),
                       point + QPointF(radius * 0.55, -radius), point + QPointF(radius, 0));
        painter.drawPath(bridge);
    }
}

} // namespace afs

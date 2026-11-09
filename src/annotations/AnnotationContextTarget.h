#pragma once
#include <QPointF>
#include <QString>
class QGraphicsItem;
namespace afs {
class FlowsheetScene;
struct AnnotationContextTarget {
    QGraphicsItem* item{nullptr};
    QString streamId;
    QString reagentId;
    bool line{false};
};
AnnotationContextTarget annotationContextTarget(const FlowsheetScene& scene, const QPointF& position);
}

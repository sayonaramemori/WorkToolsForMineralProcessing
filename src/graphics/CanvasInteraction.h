#pragma once
#include <QGraphicsScene>

namespace afs {
class FlotationUnitItem;
class ProductLineItem;
class MergeJunctionItem;
class InputLineItem;

// Graphics-owned ports. Items request synchronous geometry/connection work
// without knowing a concrete editor scene or retaining callbacks into it.
class CanvasGeometryHost {
public:
    virtual ~CanvasGeometryHost() = default;
    virtual void refreshConnections() = 0;
    virtual void sourceWidthChanged(FlotationUnitItem* source, double oldWidth) = 0;
    virtual void moveConnectedPeers(FlotationUnitItem* source, const QPointF& delta) = 0;
    virtual void notifyTopologyChanged() = 0;
    virtual void notifyRouteChanged() = 0;
};
class CanvasConnectionHost {
public:
    virtual ~CanvasConnectionHost() = default;
    virtual InputLineItem* inputAtDropPosition(const QPointF& position) const = 0;
    virtual bool connectProduct(ProductLineItem* product, InputLineItem* input, bool align = false) = 0;
    virtual bool connectMerge(MergeJunctionItem* merge, InputLineItem* input, bool align = false) = 0;
    virtual MergeJunctionItem* mergeProducts(ProductLineItem* first, ProductLineItem* second,
                                            const QString& id = {}) = 0;
    virtual bool addProductToMerge(ProductLineItem* product, MergeJunctionItem* merge) = 0;
};
inline CanvasGeometryHost* canvasGeometryHost(QGraphicsScene* scene) {
    return dynamic_cast<CanvasGeometryHost*>(scene);
}
inline CanvasConnectionHost* canvasConnectionHost(QGraphicsScene* scene) {
    return dynamic_cast<CanvasConnectionHost*>(scene);
}
} // namespace afs

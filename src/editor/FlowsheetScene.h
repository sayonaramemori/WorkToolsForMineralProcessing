#pragma once

#include <QGraphicsScene>
#include <QSet>

namespace afs {

class FlotationUnitItem;
class FeedJunctionItem;
class ProductLineItem;
class InputLineItem;
class MergeJunctionItem;

class FlowsheetScene final : public QGraphicsScene {
    Q_OBJECT
public:
    explicit FlowsheetScene(QObject* parent = nullptr);

    bool connectProduct(ProductLineItem* product, InputLineItem* input);
    bool connectProductDirect(ProductLineItem* product, InputLineItem* input);
    bool disconnectProduct(ProductLineItem* product);
    FeedJunctionItem* connectRecycle(ProductLineItem* product, InputLineItem* input,
                                     const QString& id = {});
    FeedJunctionItem* connectMergedProduct(MergeJunctionItem* merge, InputLineItem* input,
                                           const QString& id = {});
    bool connectMerge(MergeJunctionItem* merge, InputLineItem* input);
    bool connectMergeDirect(MergeJunctionItem* merge, InputLineItem* input);
    bool disconnectMerge(MergeJunctionItem* merge);
    bool disconnectRecycle(FeedJunctionItem* junction);
    MergeJunctionItem* mergeProducts(ProductLineItem* first, ProductLineItem* second,
                                     const QString& id = {});
    bool addProductToMerge(ProductLineItem* product, MergeJunctionItem* junction);
    bool splitMerge(MergeJunctionItem* junction);
    void moveConnectedPeers(FlotationUnitItem* movedUnit, const QPointF& delta);
    void refreshConnections();
    void sourceWidthChanged(FlotationUnitItem* source, double oldWidth);
    bool adjustConnectionLength(ProductLineItem* product, double delta);
    [[nodiscard]] InputLineItem* inputAtDropPosition(const QPointF& scenePosition) const;
    void refreshAppearance();
    void resetGeneratedIds();
    void notifyTopologyChanged() { emit topologyChanged(); }
    void notifyRouteChanged() { emit geometryChanged(); }

signals:
    void topologyChanged();
    void geometryChanged();

protected:
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    bool m_movingComponent{false};
    int m_nextMergeId{1};
    int m_nextFeedJunctionId{1};
    [[nodiscard]] QSet<FlotationUnitItem*> connectedComponent(FlotationUnitItem* start) const;
    void moveComponent(FlotationUnitItem* start, const QPointF& delta, bool includeStart);
};

} // namespace afs

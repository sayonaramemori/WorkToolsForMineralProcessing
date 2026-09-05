#pragma once

#include <QGraphicsPathItem>
#include <QHash>
#include <QString>
#include <QVector>
#include <optional>

namespace afs {

class FlotationUnitItem;
class MergeJunctionItem;
class ProductLineItem;

class FeedJunctionItem final : public QGraphicsPathItem {
public:
    FeedJunctionItem(QString id, ProductLineItem* recycleProduct,
                     FlotationUnitItem* targetUnit,
                     ProductLineItem* processProduct = nullptr,
                     MergeJunctionItem* processMerge = nullptr);
    FeedJunctionItem(QString id, MergeJunctionItem* recycleMerge,
                     FlotationUnitItem* targetUnit,
                     ProductLineItem* processProduct = nullptr,
                     MergeJunctionItem* processMerge = nullptr);

    [[nodiscard]] const QString& id() const { return m_id; }
    [[nodiscard]] QString externalFeedStreamId() const { return m_id + ":external-feed"; }
    [[nodiscard]] QString outputStreamId() const { return m_id + ":output"; }
    // The singular accessors are retained for old callers and project files.
    [[nodiscard]] ProductLineItem* recycleProduct() const { return m_recycleProducts.value(0); }
    [[nodiscard]] MergeJunctionItem* recycleMerge() const { return m_recycleMerges.value(0); }
    [[nodiscard]] const QVector<ProductLineItem*>& recycleProducts() const {
        return m_recycleProducts;
    }
    [[nodiscard]] const QVector<MergeJunctionItem*>& recycleMerges() const {
        return m_recycleMerges;
    }
    bool addRecycleProduct(ProductLineItem* product);
    bool addRecycleMerge(MergeJunctionItem* merge);
    [[nodiscard]] ProductLineItem* processProduct() const { return m_processProduct; }
    [[nodiscard]] MergeJunctionItem* processMerge() const { return m_processMerge; }
    [[nodiscard]] bool hasExternalFeed() const {
        return !m_processProduct && !m_processMerge;
    }
    [[nodiscard]] FlotationUnitItem* targetUnit() const { return m_targetUnit; }
    [[nodiscard]] QPointF junctionPosition() const { return m_junctionPosition; }
    [[nodiscard]] QString recycleStreamId() const;
    [[nodiscard]] QPointF sourceAnnotationAnchor(const QString& streamId) const {
        return m_sourceAnnotationAnchors.value(streamId, m_junctionPosition);
    }
    [[nodiscard]] const QHash<QString, double>& manualRouteXs() const { return m_manualRouteXs; }
    [[nodiscard]] const QHash<QString, double>& manualRouteYs() const { return m_manualRouteYs; }
    void setManualRouteX(const QString& streamId, std::optional<double> x);
    void setManualRouteY(const QString& streamId, std::optional<double> y);
    void resetManualRoutes();
    [[nodiscard]] QString selectedSourceStreamId() const { return m_selectedStreamId; }
    [[nodiscard]] QPointF recycleAnchor() const { return m_recycleAnchor; }
    [[nodiscard]] QPointF externalFeedAnchor() const { return m_externalFeedAnchor; }
    [[nodiscard]] QPointF outputAnchor() const { return m_outputAnchor; }
    [[nodiscard]] QPointF processAnnotationAnchor() const {
        return m_processAnnotationAnchor;
    }

    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    void updatePath();
    void refreshAppearance();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QString m_id;
    QVector<ProductLineItem*> m_recycleProducts;
    QVector<MergeJunctionItem*> m_recycleMerges;
    ProductLineItem* m_processProduct{nullptr};
    MergeJunctionItem* m_processMerge{nullptr};
    FlotationUnitItem* m_targetUnit;
    QPointF m_junctionPosition;
    QPointF m_recycleAnchor;
    QPointF m_externalFeedAnchor;
    QPointF m_outputAnchor;
    QPointF m_processAnnotationAnchor;
    QPainterPath m_linePath;
    QPainterPath m_commonPath;
    QPainterPath m_arrowPath;
    QHash<QString, QPainterPath> m_sourcePaths;
    QHash<QString, QPainterPath> m_sourceArrowPaths;
    QHash<QString, QPointF> m_sourceAnnotationAnchors;
    QHash<QString, QPointF> m_verticalHandles;
    QHash<QString, QPointF> m_horizontalHandles;
    QHash<QString, double> m_manualRouteXs;
    QHash<QString, double> m_manualRouteYs;
    QString m_selectedStreamId;
    QString m_editingStreamId;
    enum class EditingAxis { None, Horizontal, Vertical };
    EditingAxis m_editingAxis{EditingAxis::None};

    [[nodiscard]] QPointF processSourceAnchor() const;
    void appendSourcePath(QPainterPath& path, const QString& streamId, const QPointF& start,
                          const QPointF& end, bool routeLeft, int routeIndex, int entryIndex);
};

} // namespace afs

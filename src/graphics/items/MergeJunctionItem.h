#pragma once

#include <QGraphicsPathItem>
#include <QString>
#include <QVector>
#include "annotations/AnnotationTypes.h"

class QGraphicsSimpleTextItem;

namespace afs {

class ProductLineItem;
class FeedJunctionItem;
class InputLineItem;
class FlotationUnitItem;

class MergeJunctionItem final : public QGraphicsPathItem {
public:
    MergeJunctionItem(QString id, ProductLineItem* first, ProductLineItem* second);

    [[nodiscard]] ProductLineItem* firstProduct() const { return m_products.value(0); }
    [[nodiscard]] ProductLineItem* secondProduct() const { return m_products.value(1); }
    [[nodiscard]] const QVector<ProductLineItem*>& products() const { return m_products; }
    bool addProduct(ProductLineItem* product);
    [[nodiscard]] double mergeX() const { return m_mergeX; }
    [[nodiscard]] double mergeY() const { return m_mergeY; }
    [[nodiscard]] const QString& id() const { return m_id; }
    [[nodiscard]] QString outputStreamId() const { return m_id + ":output"; }
    [[nodiscard]] QPointF mergePosition() const { return {m_mergeX, m_mergeY}; }
    [[nodiscard]] QPointF outputEndPosition() const { return m_outputEnd; }
    [[nodiscard]] FeedJunctionItem* feedJunction() const { return m_feedJunction; }
    [[nodiscard]] FlotationUnitItem* targetUnit() const { return m_targetUnit; }
    [[nodiscard]] bool isConnected() const { return m_targetUnit != nullptr; }
    [[nodiscard]] bool isAvailable() const { return !m_feedJunction && !m_targetUnit; }

    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    void updatePath();
    void refreshAppearance();
    void setFeedJunction(FeedJunctionItem* junction);
    void setTargetUnit(FlotationUnitItem* target);
    void setProductName(const QString& name);
    void setTextSettings(const AnnotationTextSettings& settings);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    QString m_id;
    QVector<ProductLineItem*> m_products;
    double m_mergeX{0.0};
    double m_mergeY{0.0};
    QPainterPath m_linePath;
    QPainterPath m_arrowPath;
    QPointF m_outputEnd;
    bool m_dragging{false};
    QPointF m_dragEnd;
    InputLineItem* m_highlightedInput{nullptr};
    FeedJunctionItem* m_feedJunction{nullptr};
    FlotationUnitItem* m_targetUnit{nullptr};
    QGraphicsSimpleTextItem* m_nameLabel{nullptr};
    AnnotationTextSettings m_textSettings;

    void setHighlightedInput(InputLineItem* input);
};

} // namespace afs

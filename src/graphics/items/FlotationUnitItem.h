#pragma once

#include "core/FlotationUnit.h"

#include <QGraphicsItem>

namespace afs {

class FlotationUnitItem final : public QGraphicsItem {
public:
    explicit FlotationUnitItem(FlotationUnit unit);

    [[nodiscard]] QRectF boundingRect() const override;
    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    [[nodiscard]] const FlotationUnit& unit() const { return m_unit; }
    bool adjustWidth(double delta);
    bool adjustInputLength(double delta);
    [[nodiscard]] double symbolWidth() const {
        return m_unit.kind == UnitKind::BallMill ? m_unit.ballMillDiameter : m_unit.width;
    }
    bool setLeftSplitPercent(double percent);
    [[nodiscard]] class InputLineItem* inputLine() const { return m_inputLine; }
    [[nodiscard]] QPointF inputAnchorScenePosition() const;
    [[nodiscard]] QList<class ProductLineItem*> products() const;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    [[nodiscard]] QString poolDisplayLabel() const;

    FlotationUnit m_unit;
    class InputLineItem* m_inputLine{nullptr};
    class ProductLineItem* m_leftProduct{nullptr};
    class ProductLineItem* m_middleProduct{nullptr};
    class ProductLineItem* m_rightProduct{nullptr};
};

} // namespace afs

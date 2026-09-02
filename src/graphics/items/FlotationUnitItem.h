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
    [[nodiscard]] class InputLineItem* inputLine() const { return m_inputLine; }
    [[nodiscard]] QList<class ProductLineItem*> products() const;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    FlotationUnit m_unit;
    class InputLineItem* m_inputLine;
    class ProductLineItem* m_leftProduct;
    class ProductLineItem* m_rightProduct;
};

} // namespace afs

#pragma once

#include <QGraphicsLineItem>

namespace afs {

class FlotationUnitItem;
class FeedJunctionItem;
class ProductLineItem;
class MergeJunctionItem;

class InputLineItem final : public QGraphicsLineItem {
public:
    explicit InputLineItem(FlotationUnitItem* unit);

    [[nodiscard]] QPainterPath shape() const override;

    [[nodiscard]] FlotationUnitItem* unit() const { return m_unit; }
    [[nodiscard]] ProductLineItem* sourceProduct() const { return m_sourceProduct; }
    [[nodiscard]] MergeJunctionItem* sourceMerge() const { return m_sourceMerge; }
    [[nodiscard]] FeedJunctionItem* feedJunction() const { return m_feedJunction; }
    void setSourceProduct(ProductLineItem* product);
    void setSourceMerge(MergeJunctionItem* merge);
    void setFeedJunction(FeedJunctionItem* junction);
    void setDropHighlighted(bool highlighted);
    void refreshAppearance();

private:
    FlotationUnitItem* m_unit;
    ProductLineItem* m_sourceProduct{nullptr};
    MergeJunctionItem* m_sourceMerge{nullptr};
    FeedJunctionItem* m_feedJunction{nullptr};
    bool m_dropHighlighted{false};
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
};

} // namespace afs

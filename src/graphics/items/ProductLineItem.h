#pragma once

#include <QGraphicsPathItem>
#include <QString>
#include <optional>
#include "annotations/AnnotationTypes.h"

class QGraphicsSimpleTextItem;

namespace afs {

class FlotationUnitItem;
class FeedJunctionItem;
class InputLineItem;
class MergeJunctionItem;

enum class ProductSide { Left = -1, Middle = 0, Right = 1 };

[[nodiscard]] inline QString productSideSuffix(ProductSide side) {
    switch (side) {
    case ProductSide::Left: return ":left";
    case ProductSide::Middle: return ":middle";
    case ProductSide::Right: return ":right";
    }
    return {};
}

[[nodiscard]] inline QString productSideLabel(ProductSide side) {
    switch (side) {
    case ProductSide::Left: return QStringLiteral("左");
    case ProductSide::Middle: return QStringLiteral("中");
    case ProductSide::Right: return QStringLiteral("右");
    }
    return {};
}

[[nodiscard]] inline int productSideDirection(ProductSide side) {
    return static_cast<int>(side);
}

class ProductLineItem final : public QGraphicsPathItem {
public:
    ProductLineItem(FlotationUnitItem* sourceUnit, ProductSide side);

    [[nodiscard]] FlotationUnitItem* sourceUnit() const { return m_sourceUnit; }
    [[nodiscard]] FlotationUnitItem* targetUnit() const { return m_targetUnit; }
    [[nodiscard]] ProductSide side() const { return m_side; }
    [[nodiscard]] const QString& streamId() const { return m_streamId; }
    [[nodiscard]] bool isConnected() const { return m_targetUnit != nullptr; }
    [[nodiscard]] bool isMerged() const { return m_mergeJunction != nullptr; }
    [[nodiscard]] bool isRecycled() const { return m_feedJunction != nullptr; }
    [[nodiscard]] bool isAvailable() const { return !isConnected() && !isMerged() && !isRecycled(); }
    [[nodiscard]] QPointF unconnectedEndScenePosition() const;
    [[nodiscard]] QPointF sourceAnchorScenePosition() const;
    [[nodiscard]] MergeJunctionItem* mergeJunction() const { return m_mergeJunction; }
    [[nodiscard]] FeedJunctionItem* feedJunction() const { return m_feedJunction; }
    [[nodiscard]] QPainterPath shape() const override;

    void setTargetUnit(FlotationUnitItem* target);
    void setMergeJunction(MergeJunctionItem* junction);
    void setFeedJunction(FeedJunctionItem* junction);
    void setDropHighlighted(bool highlighted);
    void updatePath();
    void refreshAppearance();
    void setProductName(const QString& name);
    void setTextSettings(const AnnotationTextSettings& settings);
    [[nodiscard]] std::optional<double> manualRouteY() const { return m_manualRouteY; }
    void setManualRouteY(std::optional<double> y);
    [[nodiscard]] double terminalLength() const;
    [[nodiscard]] std::optional<double> terminalLengthOverride() const {
        return m_terminalLength;
    }
    bool adjustTerminalLength(double delta);
    bool setTerminalLength(std::optional<double> length);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

private:
    FlotationUnitItem* m_sourceUnit;
    FlotationUnitItem* m_targetUnit{nullptr};
    ProductSide m_side;
    QString m_streamId;
    bool m_dragging{false};
    QPointF m_dragEnd;
    InputLineItem* m_highlightedInput{nullptr};
    ProductLineItem* m_highlightedProduct{nullptr};
    MergeJunctionItem* m_highlightedMerge{nullptr};
    MergeJunctionItem* m_mergeJunction{nullptr};
    FeedJunctionItem* m_feedJunction{nullptr};
    bool m_dropHighlighted{false};
    bool m_routeEditing{false};
    std::optional<double> m_manualRouteY;
    std::optional<double> m_terminalLength;
    QGraphicsSimpleTextItem* m_nameLabel{nullptr};
    AnnotationTextSettings m_textSettings;

    [[nodiscard]] double sideX() const;
    void setHighlightedInput(InputLineItem* input);
    void setHighlightedProduct(ProductLineItem* product);
};

} // namespace afs

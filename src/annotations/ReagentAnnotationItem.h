#pragma once

#include "annotations/AnnotationTypes.h"
#include <QGraphicsObject>

class QWidget;

namespace afs {

class ReagentAnnotationItem final : public QGraphicsObject {
    Q_OBJECT
public:
    explicit ReagentAnnotationItem(AnnotationRecord record);
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
    const AnnotationRecord& record() const { return m_record; }
    void setRecord(const AnnotationRecord& record);
    QString formattedHtml() const;
    static bool editRecord(QWidget* parent, AnnotationRecord& record, const QString& title);
    void setAnchor(const QPointF& anchor);
    void setTextSettings(const AnnotationTextSettings& settings);
    void refreshAppearance() { update(); }

signals:
    void recordEdited(const afs::AnnotationRecord& record);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    AnnotationRecord m_record;
    AnnotationTextSettings m_textSettings;
    QRectF m_textBounds;
    QPointF m_anchor;
    bool m_hasAnchor{false};
    bool m_updatingPosition{false};
    void updateBounds();
};

} // namespace afs

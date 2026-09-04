#pragma once

#include "annotations/AnnotationTypes.h"

#include <QGraphicsObject>

class QGraphicsSceneMouseEvent;

namespace afs {

class AnnotationItem final : public QGraphicsObject {
    Q_OBJECT
public:
    AnnotationItem(AnnotationRecord record, QString text);
    static bool editUserNote(QWidget* parent, AnnotationRecord& record,
                             const AnnotationTextSettings& defaults,
                             const QString& title);

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    [[nodiscard]] const AnnotationRecord& record() const { return m_record; }
    [[nodiscard]] QPointF manualOffset() const { return m_record.manualOffset; }
    [[nodiscard]] const QString& text() const { return m_text; }

    void setText(QString text);
    void setRecord(AnnotationRecord record);
    void setAnchor(const QPointF& sceneAnchor, const QPointF& defaultOffset);
    void refreshAppearance();
    void setTextSettings(AnnotationTextSettings settings);
    [[nodiscard]] const AnnotationTextSettings& textSettings() const { return m_textSettings; }

signals:
    void placementEdited(const afs::AnnotationRecord& record);
    void recordEdited(const afs::AnnotationRecord& record);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    AnnotationRecord m_record;
    QString m_text;
    QRectF m_bounds;
    QPointF m_anchor;
    bool m_hasAnchor{false};
    bool m_updatingPosition{false};
    AnnotationTextSettings m_textSettings;

    void updateBounds();
};

} // namespace afs

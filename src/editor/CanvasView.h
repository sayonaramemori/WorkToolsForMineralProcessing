#pragma once

#include <QGraphicsView>
#include <functional>
#include <utility>

namespace afs {

class CanvasView final : public QGraphicsView {
public:
    using VerticalNudgeHandler = std::function<bool(qreal)>;
    using DeleteHandler = std::function<bool()>;

    explicit CanvasView(QGraphicsScene* scene, QWidget* parent = nullptr);
    void setVerticalNudgeHandler(VerticalNudgeHandler handler) {
        m_verticalNudgeHandler = std::move(handler);
    }
    void setDeleteHandler(DeleteHandler handler) { m_deleteHandler = std::move(handler); }
    void fitAllContents();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    static constexpr qreal MinimumScale = 0.2;
    static constexpr qreal MaximumScale = 4.0;

    void expandSceneForPan(const QPoint& viewportDelta);
    void panViewportBy(const QPoint& viewportDelta);
    bool m_panning{false};
    bool m_spacePanning{false};
    bool m_spaceHeld{false};
    QPoint m_lastPanPosition;
    VerticalNudgeHandler m_verticalNudgeHandler;
    DeleteHandler m_deleteHandler;
};

} // namespace afs

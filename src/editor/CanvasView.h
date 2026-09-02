#pragma once

#include <QGraphicsView>
#include <functional>
#include <utility>

namespace afs {

class CanvasView final : public QGraphicsView {
public:
    using VerticalNudgeHandler = std::function<bool(qreal)>;

    explicit CanvasView(QGraphicsScene* scene, QWidget* parent = nullptr);
    void setVerticalNudgeHandler(VerticalNudgeHandler handler) {
        m_verticalNudgeHandler = std::move(handler);
    }

protected:
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    static constexpr qreal MinimumScale = 0.2;
    static constexpr qreal MaximumScale = 4.0;

    bool m_panning{false};
    QPoint m_lastPanPosition;
    VerticalNudgeHandler m_verticalNudgeHandler;
};

} // namespace afs

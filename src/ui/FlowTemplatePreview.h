#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QGraphicsScene;
class QStackedWidget;

namespace afs {
class CanvasView;

class FlowTemplatePreview final : public QWidget {
public:
    explicit FlowTemplatePreview(QWidget* parent = nullptr);
    void setScene(QGraphicsScene* scene, const QString& message = {});

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void fitContents();
    void scheduleFit();
    CanvasView* m_view;
    QStackedWidget* m_stack;
    QLabel* m_message;
    QPushButton* m_fit;
    bool m_autoFit{true};
    bool m_fitPending{false};
};
}

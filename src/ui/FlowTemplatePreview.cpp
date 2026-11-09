#include "ui/FlowTemplatePreview.h"
#include "editor/CanvasView.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace afs {
FlowTemplatePreview::FlowTemplatePreview(QWidget* parent) : QWidget(parent) {
    setObjectName("flowTemplatePreview"); setMinimumHeight(220);
    auto* layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0);
    auto* header = new QHBoxLayout;
    header->addWidget(new QLabel("模板预览"));
    auto* hint = new QLabel("滚轮缩放 · 中键拖动 · 只读"); header->addWidget(hint, 1);
    m_fit = new QPushButton("适应窗口"); m_fit->setObjectName("fitTemplatePreview"); header->addWidget(m_fit);
    layout->addLayout(header);
    m_stack = new QStackedWidget; layout->addWidget(m_stack, 1);
    m_message = new QLabel; m_message->setObjectName("templatePreviewMessage");
    m_message->setWordWrap(true); m_message->setAlignment(Qt::AlignCenter); m_stack->addWidget(m_message);
    m_view = new CanvasView(nullptr); m_view->setObjectName("templatePreviewView");
    m_view->setInteractive(false); m_view->setContextMenuPolicy(Qt::NoContextMenu);
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    m_view->setZoomRange(0.001, 8.0);
    m_view->installEventFilter(this); m_view->viewport()->installEventFilter(this);
    m_stack->addWidget(m_view);
    connect(m_fit, &QPushButton::clicked, this, [this] { m_autoFit = true; fitContents(); });
    setScene(nullptr);
}

void FlowTemplatePreview::setScene(QGraphicsScene* scene, const QString& message) {
    m_view->setScene(scene); m_view->resetTransform(); m_autoFit = true;
    m_message->setText(message.isEmpty() ? "请选择一个模板查看预览" : message);
    m_stack->setCurrentWidget(scene ? static_cast<QWidget*>(m_view) : m_message);
    m_fit->setEnabled(scene != nullptr);
    if (scene) scheduleFit();
}

void FlowTemplatePreview::fitContents() {
    if (!m_view->scene() || !m_view->isVisible()) return;
    QRectF bounds;
    for (auto* item : m_view->scene()->items())
        if (item->isVisible()) bounds = bounds.united(item->sceneBoundingRect());
    if (bounds.isEmpty()) return;
    const qreal padding = std::max<qreal>(24, std::max(bounds.width(), bounds.height()) * 0.05);
    bounds.adjust(-padding, -padding, padding, padding);
    m_view->scene()->setSceneRect(bounds);
    m_view->fitInView(bounds, Qt::KeepAspectRatio);
    m_view->setZoomRange(std::min<qreal>(0.001, m_view->transform().m11() / 4), 8.0);
}

void FlowTemplatePreview::scheduleFit() {
    if (m_fitPending || !m_autoFit) return;
    m_fitPending = true;
    QTimer::singleShot(0, this, [this] { m_fitPending = false; if (m_autoFit) fitContents(); });
}

bool FlowTemplatePreview::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride) {
        auto* key = static_cast<QKeyEvent*>(event);
        // Window-wide edit shortcuts must not edit the main canvas while
        // keyboard focus is inside this read-only preview.
        if (key->matches(QKeySequence::Undo) || key->matches(QKeySequence::Redo)
            || key->matches(QKeySequence::Paste) || key->matches(QKeySequence::Cut)
            || key->matches(QKeySequence::Copy) || key->key() == Qt::Key_Delete
            || (key->modifiers().testFlag(Qt::ControlModifier)
                && (key->key() == Qt::Key_Plus || key->key() == Qt::Key_Equal
                    || key->key() == Qt::Key_Minus || key->key() == Qt::Key_B))) {
            event->accept(); return true;
        }
    }
    if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Home) {
        m_autoFit = true; fitContents(); return true;
    }
    if (event->type() == QEvent::Wheel || (event->type() == QEvent::MouseButtonPress
        && static_cast<QMouseEvent*>(event)->button() == Qt::MiddleButton)) m_autoFit = false;
    return QWidget::eventFilter(watched, event);
}

void FlowTemplatePreview::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); scheduleFit(); }
void FlowTemplatePreview::showEvent(QShowEvent* event) { QWidget::showEvent(event); scheduleFit(); }
}

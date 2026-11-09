#include "ui/WorkspacePanelSelector.h"
#include "ui/WorkspaceSidebar.h"

#include <QAbstractItemView>
#include <QAction>
#include <QSignalBlocker>
#include <QWheelEvent>
#include <algorithm>

namespace afs {
WorkspacePanelSelector::WorkspacePanelSelector(WorkspaceSidebar& sidebar, QWidget* parent)
    : QComboBox(parent) {
    setObjectName("workspacePanelCombo");
    setAccessibleName(tr("右侧面板"));
    setToolTip(tr("切换右侧面板：下拉选择，或将鼠标移到此处滚动滚轮"));
    setMinimumContentsLength(6);
    setMinimumWidth(110);
    setFocusPolicy(Qt::StrongFocus);
    for (const auto& key : sidebar.panelKeys()) addItem(sidebar.panelAction(key)->text(), key);
    setCurrentIndex(findData(sidebar.currentKey()));
    connect(this, &QComboBox::currentIndexChanged, &sidebar, [this, &sidebar](int index) {
        if (index >= 0) sidebar.showPanel(itemData(index).toString());
    });
    connect(&sidebar, &WorkspaceSidebar::panelChanged, this, [this](const QString& key) {
        const QSignalBlocker blocker(this);
        setCurrentIndex(findData(key));
        m_wheelRemainder = 0;
    });
}

void WorkspacePanelSelector::wheelEvent(QWheelEvent* event) {
    if (event->phase() == Qt::ScrollBegin) m_wheelRemainder = 0;
    if (view()->isVisible()) {
        m_wheelRemainder = 0;
        QComboBox::wheelEvent(event);
        return;
    }
    const int delta = event->angleDelta().y();
    if (!isEnabled() || !count() || !delta) {
        if (event->phase() == Qt::ScrollEnd) m_wheelRemainder = 0;
        event->ignore();
        return;
    }
    if ((m_wheelRemainder > 0 && delta < 0)
        || (m_wheelRemainder < 0 && delta > 0)) m_wheelRemainder = 0;
    m_wheelRemainder += delta;
    const int steps = m_wheelRemainder / 120;
    const int remainder = m_wheelRemainder % 120;
    if (steps) setCurrentIndex(std::clamp(currentIndex() - steps, 0, count() - 1));
    m_wheelRemainder = remainder;
    if (event->phase() == Qt::ScrollEnd) m_wheelRemainder = 0;
    // Hovering is sufficient; do not steal keyboard focus or pass the wheel
    // through to a canvas/panel behind this control.
    event->accept();
}
}

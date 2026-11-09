#include "ui/WorkspaceSidebar.h"

#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace afs {
namespace {
void embedPanel(QDockWidget* panel) {
    panel->setFeatures(QDockWidget::NoDockWidgetFeatures);
    panel->setAllowedAreas(Qt::NoDockWidgetArea);
    auto* title = new QWidget(panel); title->setFixedHeight(0);
    panel->setTitleBarWidget(title);
    // Native dock menus must not bypass exclusive page switching.
    panel->toggleViewAction()->setVisible(false);
    panel->toggleViewAction()->setEnabled(false);
}
}

WorkspaceSidebar::WorkspaceSidebar(QWidget* parent) : QDockWidget("功能面板", parent) {
    setObjectName("workspaceSidebar"); setFeatures(NoDockWidgetFeatures);
    setAllowedAreas(Qt::RightDockWidgetArea);
    auto* title = new QWidget(this); title->setFixedHeight(0); setTitleBarWidget(title);
    toggleViewAction()->setVisible(false); toggleViewAction()->setEnabled(false);
    m_stack = new QStackedWidget; m_stack->setObjectName("workspacePanelStack");
    m_actions = new QActionGroup(this); m_actions->setExclusive(true);
    auto* body = new QWidget; auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    layout->addWidget(m_stack, 7);
    m_logArea = new QWidget; m_logArea->setObjectName("workspaceLogArea");
    m_logLayout = new QVBoxLayout(m_logArea); m_logLayout->setContentsMargins(8, 4, 8, 4); m_logLayout->setSpacing(2);
    auto* logTitle = new QLabel("操作日志"); auto font = logTitle->font(); font.setBold(true); logTitle->setFont(font);
    m_logLayout->addWidget(logTitle);
    layout->addWidget(m_logArea, 1); m_logArea->hide();
    setWidget(body);
    body->installEventFilter(this);
    m_logArea->setFixedHeight(body->height() / 8);
}

bool WorkspaceSidebar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == widget() && event->type() == QEvent::Resize)
        m_logArea->setFixedHeight(static_cast<QResizeEvent*>(event)->size().height() / 8);
    return QDockWidget::eventFilter(watched, event);
}

void WorkspaceSidebar::setLogPanel(QDockWidget* panel) {
    if (!panel || m_logPanel == panel) return;
    if (m_logPanel) { m_logLayout->removeWidget(m_logPanel); m_logPanel->hide(); m_logPanel->setParent(this); }
    embedPanel(panel);
    // The log is a compact, scrollable strip; its editor's minimum size hint
    // must not override the height assigned by the workspace.
    panel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    panel->setMinimumHeight(0);
    m_logPanel = panel; m_logLayout->addWidget(panel, 1); m_logArea->show();
}

void WorkspaceSidebar::addPanel(const QString& key, const QString& label, QDockWidget* panel) {
    if (!panel || panelAction(key)) return;
    embedPanel(panel); m_stack->addWidget(panel);
    auto* action = new QAction(label, this); action->setCheckable(true);
    action->setObjectName("workspaceAction_" + key); m_actions->addAction(action);
    m_pages.append({key, panel, action});
    connect(action, &QAction::triggered, this, [this, key] { showPanel(key); });
}

bool WorkspaceSidebar::showPanel(const QString& key) {
    const auto previous = currentKey();
    for (const auto& page : m_pages) if (page.key == key && page.panel) {
        m_stack->setCurrentWidget(page.panel); page.panel->show(); page.action->setChecked(true);
        for (const auto& other : m_pages) if (other.panel && other.panel != page.panel) other.panel->hide();
        if (previous != key) emit panelChanged(key);
        return true;
    }
    return false;
}

QString WorkspaceSidebar::currentKey() const {
    for (const auto& page : m_pages) if (page.panel && page.panel == m_stack->currentWidget()) return page.key;
    return {};
}

QDockWidget* WorkspaceSidebar::currentPanel() const { return qobject_cast<QDockWidget*>(m_stack->currentWidget()); }

QAction* WorkspaceSidebar::panelAction(const QString& key) const {
    for (const auto& page : m_pages) if (page.key == key) return page.action;
    return nullptr;
}

QStringList WorkspaceSidebar::panelKeys() const {
    QStringList keys;
    for (const auto& page : m_pages) if (page.panel) keys.append(page.key);
    return keys;
}

} // namespace afs

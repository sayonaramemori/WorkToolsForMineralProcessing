#pragma once

#include <QDockWidget>
#include <QPointer>
#include <QStringList>
#include <QVector>

class QAction;
class QActionGroup;
class QStackedWidget;
class QVBoxLayout;

namespace afs {

// Hosts value/editor panels in one right-hand area. Switching a page changes
// only widget visibility; panel instances and their current state are retained.
class WorkspaceSidebar final : public QDockWidget {
    Q_OBJECT
public:
    explicit WorkspaceSidebar(QWidget* parent = nullptr);
    void addPanel(const QString& key, const QString& label, QDockWidget* panel);
    void setLogPanel(QDockWidget* panel);
    bool showPanel(const QString& key);
    QString currentKey() const;
    QDockWidget* currentPanel() const;
    QAction* panelAction(const QString& key) const;
    QStringList panelKeys() const;

signals:
    void panelChanged(const QString& key);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Page { QString key; QPointer<QDockWidget> panel; QAction* action; };
    QVector<Page> m_pages;
    QStackedWidget* m_stack;
    QActionGroup* m_actions;
    QWidget* m_logArea;
    QVBoxLayout* m_logLayout;
    QPointer<QDockWidget> m_logPanel;
};

} // namespace afs

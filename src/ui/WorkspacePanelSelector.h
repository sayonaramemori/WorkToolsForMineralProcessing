#pragma once

#include <QComboBox>

namespace afs {
class WorkspaceSidebar;

// Toolbar entry point for the sidebar's existing pages and switching state.
class WorkspacePanelSelector final : public QComboBox {
public:
    explicit WorkspacePanelSelector(WorkspaceSidebar& sidebar, QWidget* parent = nullptr);

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    int m_wheelRemainder{0};
};
}

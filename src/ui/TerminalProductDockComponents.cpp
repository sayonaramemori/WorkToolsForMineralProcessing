#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"
#include <QInputDialog>
#include <utility>

namespace afs {

void TerminalProductDock::editComponents() {
    QStringList names;
    for (const auto& component : m_model->tableState().components) names.append(component.name);
    bool accepted = false;
    const QString text = QInputDialog::getMultiLineText(
        this, tr("组分设置"), tr("每行填写一个组分名称（例如 Cu、Pb、Zn）"),
        names.join('\n'), &accepted);
    if (!accepted) return;
    m_session.submit(EditTableComponents{text.split('\n')});
}

} // namespace afs

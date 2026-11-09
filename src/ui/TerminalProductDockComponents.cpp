#include "ui/TerminalProductDock.h"
#include "document/FlowsheetDocument.h"
#include <QInputDialog>
#include <utility>

namespace afs {

void TerminalProductDock::editComponents() {
    QStringList names;
    for (const auto& component : m_document.components()) names.append(component.name);
    bool accepted = false;
    const QString text = QInputDialog::getMultiLineText(
        this, tr("组分设置"), tr("每行填写一个组分名称（例如 Cu、Pb、Zn）"),
        names.join('\n'), &accepted);
    if (!accepted) return;
    QStringList requested;
    for (const auto& line : text.split('\n')) {
        const QString name = line.simplified().left(40);
        if (!name.isEmpty() && !requested.contains(name)) requested.append(name);
    }
    if (requested.isEmpty()) return;
    QVector<ComponentDefinition> components;
    for (int i = 0; i < requested.size(); ++i) {
        const QString id = i < m_document.components().size()
            ? m_document.components()[i].id : QString("component-%1").arg(i + 1);
        components.append({id, requested[i]});
    }
    m_document.setComponents(std::move(components));
}

} // namespace afs

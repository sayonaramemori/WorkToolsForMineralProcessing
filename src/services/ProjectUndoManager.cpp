#include "services/ProjectUndoManager.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "services/ProjectSerializer.h"

#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>
#include <utility>

namespace afs {

class SnapshotUndoCommand final : public QUndoCommand {
public:
    SnapshotUndoCommand(ProjectUndoManager& manager, QByteArray before,
                        QByteArray after, QString description)
        : QUndoCommand(std::move(description)), m_manager(manager),
          m_before(std::move(before)), m_after(std::move(after)) {}

    void undo() override { m_manager.restore(m_before); }
    void redo() override {
        if (m_initialPush) {
            m_initialPush = false;
            return;
        }
        m_manager.restore(m_after);
    }

private:
    ProjectUndoManager& m_manager;
    QByteArray m_before;
    QByteArray m_after;
    bool m_initialPush{true};
};

ProjectUndoManager::ProjectUndoManager(
    FlowsheetScene& scene, FlowsheetDocument& document,
    std::function<void()> beforeRestore, std::function<void()> afterRestore,
    QObject* parent)
    : QObject(parent), m_scene(scene), m_document(document),
      m_stack(new QUndoStack(this)), m_timer(new QTimer(this)),
      m_beforeRestore(std::move(beforeRestore)), m_afterRestore(std::move(afterRestore)) {
    m_stack->setUndoLimit(50);
    m_timer->setSingleShot(true);
    m_timer->setInterval(200);
    connect(m_timer, &QTimer::timeout, this, &ProjectUndoManager::commitPending);
}

void ProjectUndoManager::initialize() {
    m_timer->stop();
    m_stack->clear();
    m_currentSnapshot = ProjectSerializer::serialize(m_scene, m_document);
    m_pendingDescription.clear();
}

void ProjectUndoManager::scheduleCheckpoint(const QString& description) {
    if (m_restoring || m_currentSnapshot.isEmpty()) return;
    if (!description.isEmpty()) m_pendingDescription = description;
    m_timer->start();
}

void ProjectUndoManager::commitPending() {
    if (m_restoring || m_currentSnapshot.isEmpty()) return;
    const QByteArray next = ProjectSerializer::serialize(m_scene, m_document);
    if (next == m_currentSnapshot) {
        m_pendingDescription.clear();
        return;
    }
    const QString description = m_pendingDescription.isEmpty()
        ? QStringLiteral("编辑项目") : m_pendingDescription;
    m_stack->push(new SnapshotUndoCommand(
        *this, m_currentSnapshot, next, description));
    m_currentSnapshot = next;
    m_pendingDescription.clear();
}

void ProjectUndoManager::undo() {
    commitPending();
    if (m_stack->canUndo()) m_stack->undo();
}

void ProjectUndoManager::redo() {
    commitPending();
    if (m_stack->canRedo()) m_stack->redo();
}

void ProjectUndoManager::clearHistory() { initialize(); }

void ProjectUndoManager::markClean() {
    commitPending();
    m_stack->setClean();
}

bool ProjectUndoManager::isClean() const { return m_stack->isClean(); }

bool ProjectUndoManager::restore(const QByteArray& snapshot) {
    m_timer->stop();
    m_restoring = true;
    if (m_beforeRestore) m_beforeRestore();
    QString error;
    const bool restored = ProjectSerializer::deserialize(
        m_scene, m_document, snapshot, &error);
    if (restored) m_currentSnapshot = snapshot;
    if (m_afterRestore) m_afterRestore();
    m_restoring = false;
    if (!restored) emit restoreFailed(error);
    return restored;
}

} // namespace afs

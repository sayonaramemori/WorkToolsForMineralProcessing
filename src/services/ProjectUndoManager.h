#pragma once

#include <QByteArray>
#include <QObject>
#include <functional>

class QTimer;
class QUndoStack;

namespace afs {

class FlowsheetDocument;
class FlowsheetScene;
class SnapshotUndoCommand;

class ProjectUndoManager final : public QObject {
    Q_OBJECT
public:
    ProjectUndoManager(FlowsheetScene& scene, FlowsheetDocument& document,
                       std::function<void()> beforeRestore,
                       std::function<void()> afterRestore,
                       QObject* parent = nullptr);

    void initialize();
    void scheduleCheckpoint(const QString& description = QStringLiteral("编辑项目"));
    void clearHistory();
    void markClean();
    [[nodiscard]] bool isClean() const;
    [[nodiscard]] QUndoStack* stack() const { return m_stack; }

public slots:
    void undo();
    void redo();

signals:
    void restoreFailed(const QString& message);

private:
    friend class SnapshotUndoCommand;
    FlowsheetScene& m_scene;
    FlowsheetDocument& m_document;
    QUndoStack* m_stack;
    QTimer* m_timer;
    QByteArray m_currentSnapshot;
    QString m_pendingDescription;
    bool m_restoring{false};
    std::function<void()> m_beforeRestore;
    std::function<void()> m_afterRestore;

    void commitPending();
    bool restore(const QByteArray& snapshot);
};

} // namespace afs

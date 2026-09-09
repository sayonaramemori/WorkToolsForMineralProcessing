#pragma once

#include <QDockWidget>

class QPlainTextEdit;

namespace afs {

class OperationLogDock final : public QDockWidget {
public:
    explicit OperationLogDock(QWidget* parent = nullptr);
    void appendMessage(const QString& message);

private:
    QPlainTextEdit* m_output{nullptr};
};

} // namespace afs

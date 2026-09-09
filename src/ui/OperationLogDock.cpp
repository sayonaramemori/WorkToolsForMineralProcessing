#include "ui/OperationLogDock.h"

#include <QDateTime>
#include <QPlainTextEdit>

namespace afs {

OperationLogDock::OperationLogDock(QWidget* parent)
    : QDockWidget(tr("操作日志"), parent) {
    setObjectName("operationLogDock");
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea);
    m_output = new QPlainTextEdit(this);
    m_output->setObjectName("operationLog");
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(500);
    m_output->setPlaceholderText(tr("操作结果将在这里显示"));
    setWidget(m_output);
}

void OperationLogDock::appendMessage(const QString& message) {
    if (message.trimmed().isEmpty()) return;
    m_output->appendPlainText(QStringLiteral("[%1] %2").arg(
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message));
}

} // namespace afs

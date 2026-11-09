#pragma once
#include "services/CalculationDiagnostics.h"
#include <QDockWidget>
class QTreeWidget;
class QLabel;
namespace afs {
class CalculationIssuesDock final : public QDockWidget {
    Q_OBJECT
public:
    explicit CalculationIssuesDock(QWidget* parent = nullptr);
    void setEntries(const QVector<CalculationDiagnostic>& entries, const QString& status);
signals:
    void locateRequested(const QString& objectId, const QString& streamId,
                         const QString& componentId, bool grade, bool edit);
    void checkRequested();
private:
    QTreeWidget* m_tree;
    QLabel* m_status;
    QLabel* m_details;
};
}

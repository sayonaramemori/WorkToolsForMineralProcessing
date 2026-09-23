#pragma once

#include "annotations/AnnotationTypes.h"

#include <QToolButton>

class QActionGroup;
class QAction;

namespace afs {

class ResultMetricMenu final : public QToolButton {
    Q_OBJECT
public:
    explicit ResultMetricMenu(QWidget* parent = nullptr);
    [[nodiscard]] QMenu* metricMenu() const { return menu(); }
    void setMassUnit(MassUnit unit, const QString& customUnit = {});
    void setSettings(const AnnotationTextSettings& settings);

signals:
    void metricVisibilityChanged(afs::ResultMetric metric, bool visible);
    void productNameVisibilityChanged(bool visible);
    void metricLabelModeChanged(afs::MetricLabelMode mode);
    void massUnitChanged(afs::MassUnit unit);
    void customMassUnitChanged(const QString& unit);

private:
    QActionGroup* m_massUnitGroup{nullptr};
    QAction* m_dryMassAction{nullptr};
    QAction* m_gradeAction{nullptr};
    QAction* m_productNameAction{nullptr};
    QAction* m_yieldAction{nullptr};
    QAction* m_recoveryAction{nullptr};
    QAction* m_chineseLabelAction{nullptr};
    QAction* m_symbolLabelAction{nullptr};
    MassUnit m_massUnit{MassUnit::Gram};
    QString m_customMassUnit;
};

} // namespace afs

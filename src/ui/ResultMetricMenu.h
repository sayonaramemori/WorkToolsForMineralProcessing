#pragma once

#include "annotations/AnnotationTypes.h"

#include <QToolButton>

class QActionGroup;

namespace afs {

class ResultMetricMenu final : public QToolButton {
    Q_OBJECT
public:
    explicit ResultMetricMenu(QWidget* parent = nullptr);
    [[nodiscard]] QMenu* metricMenu() const { return menu(); }
    void setMassUnit(MassUnit unit, const QString& customUnit = {});

signals:
    void metricVisibilityChanged(afs::ResultMetric metric, bool visible);
    void metricLabelModeChanged(afs::MetricLabelMode mode);
    void massUnitChanged(afs::MassUnit unit);
    void customMassUnitChanged(const QString& unit);

private:
    QActionGroup* m_massUnitGroup{nullptr};
    MassUnit m_massUnit{MassUnit::Gram};
    QString m_customMassUnit;
};

} // namespace afs

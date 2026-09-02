#pragma once

#include "annotations/AnnotationTypes.h"

#include <QToolButton>

namespace afs {

class ResultMetricMenu final : public QToolButton {
    Q_OBJECT
public:
    explicit ResultMetricMenu(QWidget* parent = nullptr);
    [[nodiscard]] QMenu* metricMenu() const { return menu(); }

signals:
    void metricVisibilityChanged(afs::ResultMetric metric, bool visible);
    void metricLabelModeChanged(afs::MetricLabelMode mode);
};

} // namespace afs

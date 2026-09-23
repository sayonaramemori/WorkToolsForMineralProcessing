#pragma once

#include "topology/TopologyTypes.h"
#include "annotations/AnnotationTypes.h"

namespace afs {

struct ComponentDisplayValue {
    QString name;
    double gradePercent{0.0};
    const topology::ProductMetrics* overall{nullptr};
};

class AnnotationContentFormatter final {
public:
    static QString formatStreamResult(const topology::StreamValue& value,
                                      const topology::ProductMetrics* overall,
                                      const ResultAnnotationSettings& settings,
                                      const QVector<ComponentDisplayValue>& components = {},
                                      const QString& productName = {});
};

} // namespace afs

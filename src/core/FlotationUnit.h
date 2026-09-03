#pragma once

#include <QPointF>
#include <QString>

namespace afs {

enum class UnitKind { Flotation, BinarySplitter, ThreeProductFlotation };

struct FlotationUnit {
    QString id;
    QPointF position;
    double width{360.0};
    double bodyHeight{150.0};
    UnitKind kind{UnitKind::Flotation};
    double leftSplitPercent{50.0};
};

} // namespace afs

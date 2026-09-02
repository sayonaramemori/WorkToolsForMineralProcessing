#pragma once

#include <QPointF>
#include <QString>

namespace afs {

struct FlotationUnit {
    QString id;
    QPointF position;
    double width{360.0};
    double bodyHeight{150.0};
};

} // namespace afs

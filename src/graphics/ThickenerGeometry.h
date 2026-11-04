#pragma once

#include <QPainterPath>

namespace afs::ThickenerGeometry {
inline constexpr double Top = -32.0;
inline constexpr double Bottom = 52.0;
inline constexpr double RimCenterY = -22.0;
inline constexpr double ConeTop = 10.0;

inline QPainterPath walls(double width) {
    QPainterPath path(QPointF(-width / 2.0, RimCenterY));
    path.lineTo(-width / 2.0, ConeTop);
    path.lineTo(0, Bottom);
    path.lineTo(width / 2.0, ConeTop);
    path.lineTo(width / 2.0, RimCenterY);
    return path;
}

inline QRectF rim(double width) { return {-width / 2.0, Top, width, 20.0}; }

inline QPainterPath shape(double width) {
    auto body = walls(width); body.closeSubpath();
    QPainterPath top; top.addEllipse(rim(width));
    return body.united(top);
}
} // namespace afs::ThickenerGeometry

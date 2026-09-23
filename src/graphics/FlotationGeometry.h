#pragma once

namespace afs::FlotationGeometry {
inline constexpr double InputHeight = 72.0;
inline constexpr double DoubleLineGap = 14.0;
inline constexpr double TopLineWidth = 4.2;
inline constexpr double BodyLineWidth = 2.2;
inline constexpr double ArrowHeight = 18.0;
inline constexpr double ArrowHalfWidth = 7.0;
// Product and junction routes are frequently close together in dense
// flowsheets.  Keep their selection envelope only modestly wider than the
// rendered stroke, otherwise a nearby long route captures clicks intended for
// a short terminal/product route.
inline constexpr double RouteHitWidth = 7.0;

// The input line is a connection target as well as a selectable item, so it
// deliberately retains a more forgiving hit area.
inline constexpr double InputHitWidth = 12.0;
} // namespace afs::FlotationGeometry

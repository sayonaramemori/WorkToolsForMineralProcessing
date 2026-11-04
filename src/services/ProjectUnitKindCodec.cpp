#include "services/ProjectUnitKindCodec.h"
#include <array>

namespace afs::project_serialization {
namespace {
struct Entry { UnitKind kind; const char* key; };
constexpr std::array entries{
    Entry{UnitKind::Flotation, "flotation"},
    Entry{UnitKind::MagneticSeparation, "magnetic-separation"},
    Entry{UnitKind::WeakMagneticSeparation, "weak-magnetic-separation"},
    Entry{UnitKind::StrongMagneticSeparation, "strong-magnetic-separation"},
    Entry{UnitKind::SpiralChute, "spiral-chute"},
    Entry{UnitKind::ShakingTable, "shaking-table"},
    Entry{UnitKind::DenseMediumCyclone, "dense-medium-cyclone"},
    Entry{UnitKind::SedimentationTank, "sedimentation-tank"},
    Entry{UnitKind::DemediumScreen, "demedium-screen"},
    Entry{UnitKind::Screening, "screening"},
    Entry{UnitKind::Classification, "classification"},
    Entry{UnitKind::TailingsPool, "tailings-pool"},
    Entry{UnitKind::ConcentratePool, "concentrate-pool"},
    Entry{UnitKind::WaterPool, "water-pool"},
    Entry{UnitKind::MediaTank, "media-tank"},
    Entry{UnitKind::MixingTank, "mixing-tank"},
    Entry{UnitKind::BallMill, "ball-mill"},
    Entry{UnitKind::Thickener, "thickener"},
    Entry{UnitKind::BinarySplitter, "binary-splitter"},
    Entry{UnitKind::ThreeProductFlotation, "three-product-flotation"},
    Entry{UnitKind::ThreeProductScreening, "three-product-screening"},
    Entry{UnitKind::ThreeProductDemediumScreen, "three-product-demedium-screen"}
};
}

QString unitKindKey(UnitKind kind) {
    for (const auto& entry : entries) if (entry.kind == kind) return QString::fromLatin1(entry.key);
    return {};
}

std::optional<UnitKind> unitKindFromKey(const QString& key) {
    for (const auto& entry : entries) if (key == QLatin1String(entry.key)) return entry.kind;
    return std::nullopt;
}

} // namespace afs::project_serialization

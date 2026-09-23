#pragma once

#include <QPointF>
#include <QString>

namespace afs {

// "FlotationUnit" is the legacy name of the common one-in/multiple-out
// process-unit model.  UnitKind controls its process semantics and drawing.
enum class UnitKind {
    Flotation,
    // Kept for projects created before weak/strong magnetic separation was
    // introduced. New diagrams should prefer one of the explicit variants.
    MagneticSeparation,
    WeakMagneticSeparation,
    StrongMagneticSeparation,
    SpiralChute,
    ShakingTable,
    DenseMediumCyclone,
    SedimentationTank,
    DemediumScreen,
    Screening,
    Classification,
    TailingsPool,
    ConcentratePool,
    WaterPool,
    MediaTank,
    MixingTank,
    BinarySplitter,
    ThreeProductFlotation,
    ThreeProductScreening,
    ThreeProductDemediumScreen
};

[[nodiscard]] inline bool isMagneticSeparation(UnitKind kind) {
    return kind == UnitKind::MagneticSeparation
        || kind == UnitKind::WeakMagneticSeparation
        || kind == UnitKind::StrongMagneticSeparation;
}

[[nodiscard]] inline bool isStoragePool(UnitKind kind) {
    return kind == UnitKind::TailingsPool || kind == UnitKind::ConcentratePool
        || kind == UnitKind::WaterPool || kind == UnitKind::MediaTank
        || kind == UnitKind::MixingTank;
}

[[nodiscard]] inline bool isMediaTank(UnitKind kind) {
    return kind == UnitKind::MediaTank;
}

[[nodiscard]] inline bool isCylindricalTank(UnitKind kind) {
    return kind == UnitKind::MediaTank || kind == UnitKind::MixingTank;
}

// These remain ordinary two-product separation units.  Their distinction is
// visual and in the default stream names, not in the material-balance model.
[[nodiscard]] inline bool isSizingSeparation(UnitKind kind) {
    return kind == UnitKind::DemediumScreen || kind == UnitKind::Screening
        || kind == UnitKind::Classification
        || kind == UnitKind::ThreeProductScreening
        || kind == UnitKind::ThreeProductDemediumScreen;
}

[[nodiscard]] inline bool hasMiddleProduct(UnitKind kind) {
    return kind == UnitKind::ThreeProductFlotation
        || kind == UnitKind::ThreeProductScreening
        || kind == UnitKind::ThreeProductDemediumScreen;
}

[[nodiscard]] inline bool isTerminalStoragePool(UnitKind kind) {
    return kind == UnitKind::TailingsPool;
}

struct FlotationUnit {
    QString id;
    QPointF position;
    double width{360.0};
    double bodyHeight{150.0};
    UnitKind kind{UnitKind::Flotation};
    double leftSplitPercent{50.0};
    // Display-only name rendered above the liquid wave for storage pools.
    // An empty value uses the kind-specific default (tailings/concentrate/water pool).
    QString poolLabel;
};

} // namespace afs

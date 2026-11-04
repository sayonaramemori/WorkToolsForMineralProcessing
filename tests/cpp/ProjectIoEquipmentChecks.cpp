#include "ProjectIoChecks.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/ProjectSerializer.h"
#include "adapters/CanvasTopologyBuilder.h"
#include <QTemporaryDir>
#include <cmath>

namespace afs::tests {

int checkProjectEquipment(const QTemporaryDir& directory) {
    QString error;
    FlowsheetScene splitterSource;
    FlotationUnit splitterUnit{"splitter", {120, 80}};
    splitterUnit.kind = UnitKind::BinarySplitter;
    splitterUnit.leftSplitPercent = 37.5;
    splitterSource.addItem(new FlotationUnitItem(splitterUnit));
    FlowsheetDocument splitterDocument;
    const QString splitterPath = directory.filePath("splitter.afs.json");
    if (!ProjectSerializer::save(splitterSource, splitterDocument, splitterPath, &error)) return 39;
    FlowsheetScene splitterLoaded;
    FlowsheetDocument splitterLoadedDocument;
    if (!ProjectSerializer::load(
            splitterLoaded, splitterLoadedDocument, splitterPath, &error)) return 40;
    FlotationUnitItem* loadedSplitter = nullptr;
    for (auto* item : splitterLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "splitter") loadedSplitter = unit;
    if (!loadedSplitter || loadedSplitter->unit().kind != UnitKind::BinarySplitter
        || std::abs(loadedSplitter->unit().leftSplitPercent - 37.5) > 0.001) return 41;
    const auto splitterSnapshot = CanvasTopologyBuilder::build(splitterLoaded);
    if (splitterSnapshot.requiredMeasurements.size() != 1
        || splitterSnapshot.requiredMeasurements.front().streamId != "splitter:left") return 42;

    FlowsheetScene magneticSource;
    FlotationUnit magneticUnit{"magnetic", {180, 120}};
    magneticUnit.kind = UnitKind::MagneticSeparation;
    magneticSource.addItem(new FlotationUnitItem(magneticUnit));
    FlowsheetDocument magneticDocument;
    const QString magneticPath = directory.filePath("magnetic.afs.json");
    if (!ProjectSerializer::save(magneticSource, magneticDocument, magneticPath, &error)) return 50;
    FlowsheetScene magneticLoaded;
    FlowsheetDocument magneticLoadedDocument;
    if (!ProjectSerializer::load(magneticLoaded, magneticLoadedDocument, magneticPath, &error)) return 51;
    FlotationUnitItem* loadedMagnetic = nullptr;
    for (auto* item : magneticLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "magnetic") loadedMagnetic = unit;
    const auto magneticSnapshot = CanvasTopologyBuilder::build(magneticLoaded);
    if (!loadedMagnetic || loadedMagnetic->unit().kind != UnitKind::MagneticSeparation
        || loadedMagnetic->products().size() != 2
        || magneticSnapshot.interestObjects.isEmpty()
        || !magneticSnapshot.interestObjects.front().displayName.contains("磁选机")) return 52;

    FlowsheetScene strongMagneticSource;
    FlotationUnit strongMagneticUnit{"strong-magnetic", {220, 150}};
    strongMagneticUnit.kind = UnitKind::StrongMagneticSeparation;
    strongMagneticSource.addItem(new FlotationUnitItem(strongMagneticUnit));
    const QString strongMagneticPath = directory.filePath("strong-magnetic.afs.json");
    if (!ProjectSerializer::save(strongMagneticSource, magneticDocument,
                                 strongMagneticPath, &error)) return 53;
    FlowsheetScene strongMagneticLoaded;
    FlowsheetDocument strongMagneticLoadedDocument;
    if (!ProjectSerializer::load(strongMagneticLoaded, strongMagneticLoadedDocument,
                                 strongMagneticPath, &error)) return 54;
    const auto strongMagneticSnapshot = CanvasTopologyBuilder::build(strongMagneticLoaded);
    FlotationUnitItem* loadedStrongMagnetic = nullptr;
    for (auto* item : strongMagneticLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "strong-magnetic") loadedStrongMagnetic = unit;
    if (!loadedStrongMagnetic
        || loadedStrongMagnetic->unit().kind != UnitKind::StrongMagneticSeparation
        || strongMagneticSnapshot.interestObjects.isEmpty()
        || !strongMagneticSnapshot.interestObjects.front().displayName.contains("强磁选机")) return 55;

    FlowsheetScene poolSource;
    FlotationUnit tailingsPool{"tailings-pool", {0, 0}};
    tailingsPool.kind = UnitKind::TailingsPool;
    tailingsPool.poolLabel = QStringLiteral("尾矿浓缩池");
    FlotationUnit concentratePool{"concentrate-pool", {360, 0}};
    concentratePool.kind = UnitKind::ConcentratePool;
    concentratePool.poolLabel = QStringLiteral("精矿缓冲池");
    poolSource.addItem(new FlotationUnitItem(tailingsPool));
    poolSource.addItem(new FlotationUnitItem(concentratePool));
    const QString poolPath = directory.filePath("pools.afs.json");
    if (!ProjectSerializer::save(poolSource, magneticDocument, poolPath, &error)) return 56;
    FlowsheetScene poolLoaded;
    FlowsheetDocument poolLoadedDocument;
    if (!ProjectSerializer::load(poolLoaded, poolLoadedDocument, poolPath, &error)) return 57;
    FlotationUnitItem* loadedTailingsPool = nullptr;
    FlotationUnitItem* loadedConcentratePool = nullptr;
    for (auto* item : poolLoaded.items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit) {
            if (unit->unit().id == "tailings-pool") loadedTailingsPool = unit;
            if (unit->unit().id == "concentrate-pool") loadedConcentratePool = unit;
        }
    }
    if (!loadedTailingsPool || !loadedConcentratePool
        || loadedTailingsPool->unit().kind != UnitKind::TailingsPool
        || loadedTailingsPool->unit().poolLabel != QStringLiteral("尾矿浓缩池")
        || loadedTailingsPool->products().size() != 0
        || loadedConcentratePool->unit().kind != UnitKind::ConcentratePool
        || loadedConcentratePool->unit().poolLabel != QStringLiteral("精矿缓冲池")
        || loadedConcentratePool->products().size() != 1
        || loadedConcentratePool->products().front()->side() != ProductSide::Right) return 58;

    FlowsheetScene gravitySource;
    FlotationUnit spiral{"spiral", {0, 0}};
    spiral.kind = UnitKind::SpiralChute;
    FlotationUnit shakingTable{"shaking-table", {180, 0}};
    shakingTable.kind = UnitKind::ShakingTable;
    FlotationUnit cyclone{"cyclone", {360, 0}};
    cyclone.kind = UnitKind::DenseMediumCyclone;
    FlotationUnit sedimentationTank{"sedimentation-tank", {540, 0}};
    sedimentationTank.kind = UnitKind::SedimentationTank;
    FlotationUnit screening{"screening", {720, 0}};
    screening.kind = UnitKind::Screening;
    FlotationUnit classifier{"classifier", {1080, 0}};
    classifier.kind = UnitKind::Classification;
    FlotationUnit demedium{"demedium", {1440, 0}};
    demedium.kind = UnitKind::DemediumScreen;
    FlotationUnit mediaTank{"media-tank", {1800, 0}};
    mediaTank.kind = UnitKind::MediaTank;
    FlotationUnit mixingTank{"mixing-tank", {2160, 0}};
    mixingTank.kind = UnitKind::MixingTank;
    gravitySource.addItem(new FlotationUnitItem(spiral));
    gravitySource.addItem(new FlotationUnitItem(shakingTable));
    gravitySource.addItem(new FlotationUnitItem(cyclone));
    gravitySource.addItem(new FlotationUnitItem(sedimentationTank));
    gravitySource.addItem(new FlotationUnitItem(screening));
    gravitySource.addItem(new FlotationUnitItem(classifier));
    gravitySource.addItem(new FlotationUnitItem(demedium));
    gravitySource.addItem(new FlotationUnitItem(mediaTank));
    gravitySource.addItem(new FlotationUnitItem(mixingTank));
    const QString gravityPath = directory.filePath("gravity.afs.json");
    if (!ProjectSerializer::save(gravitySource, magneticDocument, gravityPath, &error)) return 59;
    FlowsheetScene gravityLoaded;
    FlowsheetDocument gravityLoadedDocument;
    if (!ProjectSerializer::load(gravityLoaded, gravityLoadedDocument, gravityPath, &error)) return 60;
    bool loadedSpiral = false;
    bool loadedShakingTable = false;
    bool loadedCyclone = false;
    bool loadedSedimentationTank = false;
    bool loadedScreening = false;
    bool loadedClassification = false;
    bool loadedDemedium = false;
    FlotationUnitItem* loadedMediaTank = nullptr;
    FlotationUnitItem* loadedMixingTank = nullptr;
    for (auto* item : gravityLoaded.items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit) {
            loadedSpiral = loadedSpiral || unit->unit().kind == UnitKind::SpiralChute;
            loadedShakingTable = loadedShakingTable || unit->unit().kind == UnitKind::ShakingTable;
            loadedCyclone = loadedCyclone || unit->unit().kind == UnitKind::DenseMediumCyclone;
            loadedSedimentationTank = loadedSedimentationTank
                || unit->unit().kind == UnitKind::SedimentationTank;
            loadedScreening = loadedScreening || unit->unit().kind == UnitKind::Screening;
            loadedClassification = loadedClassification
                || unit->unit().kind == UnitKind::Classification;
            loadedDemedium = loadedDemedium || unit->unit().kind == UnitKind::DemediumScreen;
            if (unit->unit().kind == UnitKind::MediaTank) loadedMediaTank = unit;
            if (unit->unit().kind == UnitKind::MixingTank) loadedMixingTank = unit;
        }
    }
    if (!loadedSpiral || !loadedShakingTable || !loadedCyclone || !loadedSedimentationTank || !loadedScreening || !loadedClassification
        || !loadedDemedium || !loadedMediaTank || loadedMediaTank->products().size() != 1
        || loadedMediaTank->products().front()->side() != ProductSide::Right
        || !loadedMixingTank || loadedMixingTank->products().size() != 1
        || loadedMixingTank->products().front()->side() != ProductSide::Right) return 61;

    FlowsheetScene threeProductSource;
    FlotationUnit threeProductUnit{"three", {240, 160}};
    threeProductUnit.kind = UnitKind::ThreeProductFlotation;
    threeProductSource.addItem(new FlotationUnitItem(threeProductUnit));
    FlowsheetDocument threeProductDocument;
    const QString threeProductPath = directory.filePath("three-product.afs.json");
    if (!ProjectSerializer::save(
            threeProductSource, threeProductDocument, threeProductPath, &error)) return 43;
    FlowsheetScene threeProductLoaded;
    FlowsheetDocument threeProductLoadedDocument;
    if (!ProjectSerializer::load(threeProductLoaded, threeProductLoadedDocument,
                                 threeProductPath, &error)) return 44;
    FlotationUnitItem* loadedThreeProduct = nullptr;
    for (auto* item : threeProductLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "three") loadedThreeProduct = unit;
    if (!loadedThreeProduct
        || loadedThreeProduct->unit().kind != UnitKind::ThreeProductFlotation
        || loadedThreeProduct->products().size() != 3
        || loadedThreeProduct->products()[1]->streamId() != "three:middle") return 45;
    const auto threeProductSnapshot = CanvasTopologyBuilder::build(threeProductLoaded);
    if (threeProductSnapshot.graph.streamsFrom(
            "three", topology::PortKind::MiddleProduct).size() != 1
        || threeProductSnapshot.requiredMeasurements.size() != 3) return 46;

    FlowsheetScene threeScreenSource;
    FlotationUnit threeScreenUnit{"three-screen", {240, 160}};
    threeScreenUnit.kind = UnitKind::ThreeProductScreening;
    threeScreenSource.addItem(new FlotationUnitItem(threeScreenUnit));
    const QString threeScreenPath = directory.filePath("three-product-screen.afs.json");
    if (!ProjectSerializer::save(
            threeScreenSource, threeProductDocument, threeScreenPath, &error)) return 62;
    FlowsheetScene threeScreenLoaded;
    FlowsheetDocument threeScreenLoadedDocument;
    if (!ProjectSerializer::load(threeScreenLoaded, threeScreenLoadedDocument,
                                 threeScreenPath, &error)) return 63;
    FlotationUnitItem* loadedThreeScreen = nullptr;
    for (auto* item : threeScreenLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "three-screen") loadedThreeScreen = unit;
    const auto threeScreenSnapshot = CanvasTopologyBuilder::build(threeScreenLoaded);
    if (!loadedThreeScreen
        || loadedThreeScreen->unit().kind != UnitKind::ThreeProductScreening
        || loadedThreeScreen->products().size() != 3
        || threeScreenSnapshot.graph.streamsFrom(
               "three-screen", topology::PortKind::MiddleProduct).size() != 1
        || threeScreenSnapshot.requiredMeasurements.size() != 3) return 64;

    FlowsheetScene threeDemediumSource;
    FlotationUnit threeDemediumUnit{"three-demedium", {240, 160}};
    threeDemediumUnit.kind = UnitKind::ThreeProductDemediumScreen;
    threeDemediumSource.addItem(new FlotationUnitItem(threeDemediumUnit));
    const QString threeDemediumPath = directory.filePath("three-product-demedium.afs.json");
    if (!ProjectSerializer::save(
            threeDemediumSource, threeProductDocument, threeDemediumPath, &error)) return 65;
    FlowsheetScene threeDemediumLoaded;
    FlowsheetDocument threeDemediumLoadedDocument;
    if (!ProjectSerializer::load(threeDemediumLoaded, threeDemediumLoadedDocument,
                                 threeDemediumPath, &error)) return 66;
    FlotationUnitItem* loadedThreeDemedium = nullptr;
    for (auto* item : threeDemediumLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "three-demedium") loadedThreeDemedium = unit;
    const auto threeDemediumSnapshot = CanvasTopologyBuilder::build(threeDemediumLoaded);
    if (!loadedThreeDemedium
        || loadedThreeDemedium->unit().kind != UnitKind::ThreeProductDemediumScreen
        || loadedThreeDemedium->products().size() != 3
        || threeDemediumSnapshot.graph.streamsFrom(
               "three-demedium", topology::PortKind::MiddleProduct).size() != 1
        || threeDemediumSnapshot.requiredMeasurements.size() != 3) return 67;
    return 0;
}

} // namespace afs::tests

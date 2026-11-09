#include "app/MainWindow.h"
#include "core/FlotationUnit.h"
#include <QAction>
#include <QMenu>

namespace afs {
void MainWindow::populateEquipmentMenu(QMenu* flowMenu) {
    auto* addAction = flowMenu->addAction(tr("添加浮选单元"));
    connect(addAction, &QAction::triggered, this, [this] { addFlotationUnit(); });
    auto* ballMillAction = flowMenu->addAction(tr("添加球磨机"));
    ballMillAction->setObjectName("addBallMillAction");
    connect(ballMillAction, &QAction::triggered, this, [this] { addBallMill(); });
    auto* thickenerAction = flowMenu->addAction(tr("添加浓密池"));
    thickenerAction->setObjectName("addThickenerAction");
    connect(thickenerAction, &QAction::triggered, this, [this] { addThickener(); });
    auto* magneticMenu = flowMenu->addMenu(tr("添加磁选机"));
    auto* addWeakMagneticAction = magneticMenu->addAction(tr("添加弱磁选机"));
    connect(addWeakMagneticAction, &QAction::triggered,
            this, [this] { addMagneticSeparator(UnitKind::WeakMagneticSeparation); });
    auto* addStrongMagneticAction = magneticMenu->addAction(tr("添加强磁选机"));
    connect(addStrongMagneticAction, &QAction::triggered,
            this, [this] { addMagneticSeparator(UnitKind::StrongMagneticSeparation); });
    auto* addGenericMagneticAction = magneticMenu->addAction(tr("添加通用磁选机"));
    connect(addGenericMagneticAction, &QAction::triggered,
            this, [this] { addMagneticSeparator(UnitKind::MagneticSeparation); });
    auto* gravityMenu = flowMenu->addMenu(tr("添加重选设备"));
    auto* addSpiralAction = gravityMenu->addAction(tr("添加螺旋溜槽"));
    connect(addSpiralAction, &QAction::triggered,
            this, [this] { addGravitySeparator(UnitKind::SpiralChute); });
    auto* addShakingTableAction = gravityMenu->addAction(tr("添加摇床"));
    connect(addShakingTableAction, &QAction::triggered,
            this, [this] { addGravitySeparator(UnitKind::ShakingTable); });
    auto* addDenseMediumAction = gravityMenu->addAction(tr("添加重介质旋流器"));
    connect(addDenseMediumAction, &QAction::triggered,
            this, [this] { addGravitySeparator(UnitKind::DenseMediumCyclone); });
    auto* addSedimentationAction = gravityMenu->addAction(tr("添加沉降箱"));
    connect(addSedimentationAction, &QAction::triggered,
            this, [this] { addGravitySeparator(UnitKind::SedimentationTank); });
    auto* addDemediumAction = gravityMenu->addAction(tr("添加脱介筛"));
    connect(addDemediumAction, &QAction::triggered,
            this, [this] { addGravitySeparator(UnitKind::DemediumScreen); });
    auto* addThreeProductDemediumAction = gravityMenu->addAction(tr("添加三产品脱介筛"));
    connect(addThreeProductDemediumAction, &QAction::triggered,
            this, [this] { addThreeProductDemediumScreen(); });
    auto* addMediaTankAction = gravityMenu->addAction(tr("添加介质桶"));
    connect(addMediaTankAction, &QAction::triggered,
            this, [this] { addStoragePool(UnitKind::MediaTank); });
    auto* sizingMenu = flowMenu->addMenu(tr("添加筛分分级设备"));
    auto* addScreeningAction = sizingMenu->addAction(tr("添加筛分机"));
    connect(addScreeningAction, &QAction::triggered,
            this, [this] { addSizingSeparator(UnitKind::Screening); });
    auto* addClassificationAction = sizingMenu->addAction(tr("添加分级机"));
    connect(addClassificationAction, &QAction::triggered,
            this, [this] { addSizingSeparator(UnitKind::Classification); });
    auto* addThreeProductScreenAction = sizingMenu->addAction(tr("添加三产品筛分器"));
    connect(addThreeProductScreenAction, &QAction::triggered,
            this, [this] { addThreeProductScreening(); });
    auto* poolMenu = flowMenu->addMenu(tr("添加贮池"));
    auto* addTailingsPoolAction = poolMenu->addAction(tr("添加尾矿池"));
    connect(addTailingsPoolAction, &QAction::triggered,
            this, [this] { addStoragePool(UnitKind::TailingsPool); });
    auto* addConcentratePoolAction = poolMenu->addAction(tr("添加精矿池"));
    connect(addConcentratePoolAction, &QAction::triggered,
            this, [this] { addStoragePool(UnitKind::ConcentratePool); });
    auto* addWaterPoolAction = poolMenu->addAction(tr("添加回水池"));
    connect(addWaterPoolAction, &QAction::triggered,
            this, [this] { addStoragePool(UnitKind::WaterPool); });
    auto* addMixingTankAction = poolMenu->addAction(tr("添加混料桶"));
    connect(addMixingTankAction, &QAction::triggered,
            this, [this] { addStoragePool(UnitKind::MixingTank); });
    auto* addThreeProductAction = flowMenu->addAction(tr("添加三产品浮选单元"));
    connect(addThreeProductAction, &QAction::triggered, this, [this] { addThreeProductUnit(); });
    auto* addSplitterAction = flowMenu->addAction(tr("添加二分流器"));
    connect(addSplitterAction, &QAction::triggered, this, [this] { addBinarySplitter(); });
}
} // namespace afs

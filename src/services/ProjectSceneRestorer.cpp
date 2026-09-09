#include "services/ProjectSceneRestorer.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QSet>
#include <QSignalBlocker>

namespace afs {
namespace {
void setError(QString* destination, const QString& message) {
    if (destination) *destination = message;
}
ProductLineItem* findProduct(const QHash<QString, ProductLineItem*>& products,
                             const QString& id) {
    return products.value(id, nullptr);
}
}

bool ProjectSceneRestorer::restore(const project_serialization::ProjectData& data,
                                   FlowsheetScene& scene, QString* error) {
    const QSignalBlocker blocker(&scene);
    scene.clear();
    QHash<QString, FlotationUnitItem*> units;
    QHash<QString, ProductLineItem*> products;
    QHash<QString, MergeJunctionItem*> merges;
    for (const auto& item : data.units) {
        auto* unit = new FlotationUnitItem(item.unit); scene.addItem(unit);
        units.insert(item.unit.id, unit);
        for (auto* product : unit->products()) {
            products.insert(product->streamId(), product);
            const QString key = productSideSuffix(product->side()).mid(1);
            if (item.terminalLengths.contains(key))
                product->setTerminalLength(item.terminalLengths.value(key));
        }
    }
    for (const auto& item : data.connections) {
        auto* source = findProduct(products, item.sourceStreamId);
        auto* target = units.value(item.targetUnitId, nullptr);
        if (!source || !target || !scene.connectProductDirect(source, target->inputLine())) {
            setError(error, QString("无法重建普通连接：%1").arg(item.sourceStreamId)); return false;
        }
        source->setManualRouteY(item.routeY);
    }
    for (const auto& item : data.productMerges) {
        auto* first = findProduct(products, item.streamIds[0]);
        auto* second = findProduct(products, item.streamIds[1]);
        if (merges.contains(item.id) || units.contains(item.id) || !first || !second) {
            setError(error, QString("无法重建产品合流：%1").arg(item.id)); return false;
        }
        auto* merge = scene.mergeProducts(first, second, item.id);
        if (!merge) { setError(error, QString("产品合流状态冲突：%1").arg(item.id)); return false; }
        merges.insert(item.id, merge); merge->setManualMergeY(item.mergeY);
        for (int index = 2; index < item.streamIds.size(); ++index) {
            if (!scene.addProductToMerge(findProduct(products, item.streamIds[index]), merge)) {
                setError(error, QString("无法重建产品合流支路：%1").arg(item.streamIds[index]));
                return false;
            }
        }
        if (!item.targetUnitId.isEmpty()) {
            auto* target = units.value(item.targetUnitId, nullptr);
            if (!target || !scene.connectMergeDirect(merge, target->inputLine())) {
                setError(error, QString("无法重建合流产品下游连接：%1").arg(item.id));
                return false;
            }
        }
    }
    QSet<QString> feedIds;
    for (const auto& item : data.feedJunctions) {
        auto* target = units.value(item.targetUnitId, nullptr);
        if (!target || units.contains(item.id) || merges.contains(item.id)
            || feedIds.contains(item.id)) {
            setError(error, QString("无法重建入料回流：%1").arg(item.id)); return false;
        }
        FeedJunctionItem* junction = nullptr;
        if (item.processSourceType == "product") {
            if (!scene.connectProductDirect(findProduct(products, item.processSourceId),
                                            target->inputLine())) {
                setError(error, QString("无法恢复入料汇合的上游产品：%1")
                                    .arg(item.processSourceId)); return false;
            }
        } else if (item.processSourceType == "merge") {
            if (!scene.connectMergeDirect(merges.value(item.processSourceId, nullptr),
                                          target->inputLine())) {
                setError(error, QString("无法恢复入料汇合的上游合流：%1")
                                    .arg(item.processSourceId)); return false;
            }
        }
        for (int index = 0; index < item.sourceIds.size(); ++index) {
            const QString connectionId = index == 0 ? item.id : QString{};
            if (item.sourceTypes[index] == "product")
                junction = scene.connectRecycle(findProduct(products, item.sourceIds[index]),
                                                target->inputLine(), connectionId);
            else
                junction = scene.connectMergedProduct(merges.value(item.sourceIds[index], nullptr),
                                                      target->inputLine(), connectionId);
            if (!junction) {
                setError(error, QString("入料汇合来源状态冲突：%1").arg(item.sourceIds[index]));
                return false;
            }
            const QString routeId = item.sourceTypes[index] == "product"
                ? item.sourceIds[index] : item.sourceIds[index] + ":output";
            if (index < item.routeXs.size() && item.routeXs[index])
                junction->setManualRouteX(routeId, item.routeXs[index]);
            if (index < item.routeYs.size() && item.routeYs[index])
                junction->setManualRouteY(routeId, item.routeYs[index]);
        }
        feedIds.insert(item.id);
    }
    scene.resetGeneratedIds(); scene.refreshConnections(); return true;
}

} // namespace afs

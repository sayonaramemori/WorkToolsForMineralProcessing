#include "editor/CanvasProperties.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include <QLineF>
#include <algorithm>
#include <cmath>

namespace afs {
namespace {
void lengthField(ObjectPropertyState& state, ProductLineItem* product, const QString& key, const QString& label) {
    if (product->isConnected()) {
        const double distance = product->targetUnit()->pos().y() - product->sourceUnit()->pos().y();
        state.fields.append({key, label + "纵向间距", distance, std::min(-5000.0, distance), std::max(5000.0, distance)});
    } else state.fields.append({key, label + "线长", product->terminalLength(), 30, 5000});
}
ProductLineItem* productFor(FlotationUnitItem* unit, const QString& id) {
    for (auto* product : unit->products()) if (product->streamId() == id) return product;
    return nullptr;
}
bool setLength(FlowsheetScene& scene, ProductLineItem* product, double value) {
    if (!product) return false;
    if (product->isConnected())
        return scene.adjustConnectionLength(product, value - (product->targetUnit()->pos().y() - product->sourceUnit()->pos().y()), true);
    if (!product->setTerminalLength(value)) return false;
    scene.refreshConnections(); return true;
}
}
ObjectPropertyState CanvasProperties::describe(const FlowsheetScene& scene) {
    ObjectPropertyState state;
    const auto selected = scene.selectedItems();
    if (selected.size() != 1) {
        state.title = selected.isEmpty() ? "请选择设备或线路" : QString("已选择 %1 个对象，请单选以编辑属性").arg(selected.size());
        return state;
    }
    auto* item = selected.front();
    if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
        state.objectKey = "unit/" + unit->unit().id; state.title = "设备 " + unit->unit().id;
        const bool circle = unit->unit().kind == UnitKind::BallMill;
        state.fields.append({"width", circle ? "圆圈直径" : "设备宽度", unit->symbolWidth(), circle ? 40.0 : 160.0, 720});
        state.fields.append({"input", "入料线长度", unit->inputLine()->length(), 30, 5000});
        for (auto* product : unit->products())
            lengthField(state, product, "output/" + product->streamId(), hasCenteredVerticalPorts(unit->unit().kind)
                ? QString("出料") : productSideLabel(product->side()) + "出料");
        if (unit->unit().kind == UnitKind::BinarySplitter)
            state.fields.append({"split", "左支路比例（%）", unit->unit().leftSplitPercent, 0.1, 99.9});
        if (unit->products().size() == 1) state.streamId = unit->products().front()->streamId();
    } else if (auto* input = dynamic_cast<InputLineItem*>(item)) {
        state.objectKey = "input/" + input->unit()->unit().id; state.title = "入料线 · " + input->unit()->unit().id;
        state.fields.append({"input", "入料线长度", input->length(), 30, 5000});
        if (!input->sourceProduct() && !input->sourceMerge() && !input->feedJunction()) state.streamId = input->unit()->unit().id + ":feed";
    } else if (auto* product = dynamic_cast<ProductLineItem*>(item)) {
        state.objectKey = "product/" + product->streamId(); state.title = "产品线 · " + product->streamId(); state.streamId = product->streamId();
        lengthField(state, product, "length", "出料");
        if (product->isConnected()) {
            const auto path = product->path();
            const double y = product->manualRouteY().value_or(path.elementCount() > 1 ? path.elementAt(1).y : 0);
            state.fields.append({"routeY", "横向段高度（相对设备）", y, -100000, 100000});
            state.canResetRoute = true;
        }
    } else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
        state.objectKey = "merge/" + merge->id(); state.title = "产品汇流 · " + merge->id(); state.streamId = merge->outputStreamId();
        state.fields.append({"mergeY", "汇合高度", merge->mergeY(), -100000, 100000}); state.canResetRoute = true;
    } else if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) {
        state.objectKey = "feed/" + feed->id(); state.title = "入料汇流 · " + feed->id();
        for (auto* product : feed->recycleProducts()) state.branches.append({product->streamId(), product->streamId()});
        for (auto* merge : feed->recycleMerges()) state.branches.append({merge->outputStreamId(), merge->outputStreamId()});
        state.activeBranch = feed->selectedSourceStreamId();
        if (state.activeBranch.isEmpty() && !state.branches.isEmpty()) state.activeBranch = state.branches.front().id;
        state.streamId = state.activeBranch.isEmpty() ? feed->outputStreamId() : state.activeBranch;
        if (!state.activeBranch.isEmpty()) {
            const auto anchor = feed->sourceAnnotationAnchor(state.activeBranch);
            int index = 0; for (const auto& branch : state.branches) { if (branch.id == state.activeBranch) break; ++index; }
            state.fields.append({"routeX", "支路走廊横坐标", feed->manualRouteXs().value(state.activeBranch, anchor.x()), -100000, 100000});
            state.fields.append({"feedY", "支路汇入高度", feed->manualRouteYs().value(state.activeBranch, feed->junctionPosition().y() - (index + 1) * 18), -100000, 100000});
        }
        state.canResetRoute = true;
    } else state.title = "该对象没有可编辑的设备属性";
    return state;
}
bool CanvasProperties::apply(FlowsheetScene& scene, const QString& objectKey, const QString& key, double value) {
    const auto state = describe(scene);
    if (state.objectKey.isEmpty() || state.objectKey != objectKey || !std::isfinite(value)) return false;
    bool allowed = false;
    for (const auto& field : state.fields) if (field.key == key) {
        if (std::abs(field.value - value) < 1e-9) return false;
        allowed = value >= field.minimum && value <= field.maximum;
    }
    if (!allowed) return false;
    auto* item = scene.selectedItems().front();
    if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
        if (key == "width") return unit->adjustWidth(value - unit->symbolWidth());
        if (key == "input") return unit->adjustInputLength(value - unit->inputLine()->length());
        if (key == "split") return unit->setLeftSplitPercent(value);
        if (key.startsWith("output/")) return setLength(scene, productFor(unit, key.mid(7)), value);
    }
    if (auto* input = dynamic_cast<InputLineItem*>(item); input && key == "input")
        return input->unit()->adjustInputLength(value - input->length());
    if (auto* product = dynamic_cast<ProductLineItem*>(item)) {
        if (key == "length") return setLength(scene, product, value);
        if (key == "routeY") { product->setManualRouteY(value); scene.refreshConnections(); return true; }
    }
    if (auto* merge = dynamic_cast<MergeJunctionItem*>(item); merge && key == "mergeY") {
        merge->setManualMergeY(value); scene.refreshConnections(); return true;
    }
    if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) {
        if (key == "routeX") feed->setManualRouteX(state.activeBranch, value);
        else if (key == "feedY") feed->setManualRouteY(state.activeBranch, value);
        else return false;
        scene.refreshConnections(); return true;
    }
    return false;
}
bool CanvasProperties::resetRoutes(FlowsheetScene& scene, const QString& key) {
    const auto state = describe(scene); if (state.objectKey != key || !state.canResetRoute) return false;
    auto* item = scene.selectedItems().front();
    if (auto* product = dynamic_cast<ProductLineItem*>(item)) product->setManualRouteY(std::nullopt);
    if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) merge->setManualMergeY(std::nullopt);
    if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) {
        feed->setManualRouteX(state.activeBranch, std::nullopt); feed->setManualRouteY(state.activeBranch, std::nullopt);
    }
    scene.refreshConnections(); return true;
}
bool CanvasProperties::selectBranch(FlowsheetScene& scene, const QString& key, const QString& branch) {
    if (describe(scene).objectKey != key || scene.selectedItems().size() != 1) return false;
    auto* feed = dynamic_cast<FeedJunctionItem*>(scene.selectedItems().front());
    return feed && feed->selectSourceStream(branch);
}
}

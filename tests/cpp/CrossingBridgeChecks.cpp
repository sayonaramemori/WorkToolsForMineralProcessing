#include "editor/CrossingBridgeRenderer.h"
#include "editor/FlowsheetScene.h"
#include "document/FlowsheetDocument.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/ProjectSerializer.h"
#include "services/SvgExporter.h"

#include <QBuffer>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QQueue>
#include <QRegularExpression>
#include <QSvgGenerator>
#include <QXmlStreamReader>
#include <stdexcept>

using namespace afs;

namespace {
void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

QVector<QPointF> bridgeCenters(const QByteArray& svg) {
    // Each foreground bridge is one cubic path. Its start is eight scene
    // units to the left of the actual crossing; arrows and straight lines
    // produce no cubic paths in this isolated renderer output.
    const QString number = "([+-]?[0-9]+(?:\\.[0-9]*)?(?:[eE][+-]?[0-9]+)?)";
    const QRegularExpression start("^M\\s*" + number + "[,\\s]+" + number + "\\s*C");
    QVector<QPointF> centers;
    QXmlStreamReader xml(svg);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != "path") continue;
        const auto match = start.match(xml.attributes().value("d").toString());
        if (match.hasMatch()) centers.append({match.captured(1).toDouble() + 8, match.captured(2).toDouble()});
    }
    require(!xml.hasError(), "Invalid bridge SVG");
    return centers;
}

QVector<QPointF> renderBridges(const FlowsheetScene& scene) {
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
    QSvgGenerator generator; generator.setOutputDevice(&buffer);
    generator.setSize({2000, 2000}); generator.setViewBox(QRectF(-1000, -1000, 2000, 2000));
    QPainter painter(&generator); CrossingBridgeRenderer::draw(scene, painter); painter.end();
    return bridgeCenters(bytes);
}

bool contains(const QVector<QPointF>& points, QPointF point) {
    for (const auto& candidate : points)
        if (QLineF(candidate, point).length() < 0.02) return true;
    return false;
}

void checkContinuity(const FlowsheetScene& scene, QPointF crossing, const QString& directory) {
    const auto originalPalette = QApplication::palette();
    for (bool dark : {false, true}) {
        const QColor background = dark ? QColor(32, 32, 32) : QColor(Qt::white);
        const QColor foreground = dark ? QColor(Qt::white) : QColor(Qt::black);
        auto palette = originalPalette;
        palette.setColor(QPalette::Base, background); palette.setColor(QPalette::Text, foreground);
        QApplication::setPalette(palette);
        for (int scale : {1, 4, 16}) {
            QImage image(48 * scale, 32 * scale, QImage::Format_RGB32); image.fill(background);
            QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
            painter.scale(scale, scale); painter.translate(24 - crossing.x(), 16 - crossing.y());
            painter.setPen(QPen(foreground, 2.2));
            painter.drawLine(crossing - QPointF(24, 0), crossing + QPointF(24, 0));
            painter.drawLine(crossing - QPointF(0, 16), crossing + QPointF(0, 16));
            CrossingBridgeRenderer::draw(scene, painter); painter.end();
            image.save(directory + QString("/bridge-%1-%2.png").arg(dark ? "dark" : "light").arg(scale));
            const auto preview = qEnvironmentVariable("AFS_BRIDGE_PREVIEW_PATH");
            if (!preview.isEmpty() && !dark && scale == 16) image.save(preview);
            const auto ink = [&](int x, int y) {
                if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) return false;
                const auto color = image.pixelColor(x, y);
                const int distance = std::abs(color.red() - foreground.red())
                    + std::abs(color.green() - foreground.green()) + std::abs(color.blue() - foreground.blue());
                return distance < 300;
            };
            // Follow the painted pixels from the left straight segment across
            // the arch to the right. This catches visible cap/mask gaps even
            // when the exported vector path itself contains a valid curve.
            QQueue<QPoint> queue; QSet<QPoint> visited;
            const QPoint start(2 * scale, 16 * scale), end(46 * scale, 16 * scale);
            require(ink(start.x(), start.y()) && ink(end.x(), end.y()), "Missing continuity test line");
            queue.enqueue(start); visited.insert(start);
            while (!queue.isEmpty() && !visited.contains(end)) {
                const auto current = queue.dequeue();
                for (int dx = -1; dx <= 1; ++dx) for (int dy = -1; dy <= 1; ++dy) {
                    const auto next = current + QPoint(dx, dy);
                    if (visited.contains(next) || !ink(next.x(), next.y())) continue;
                    visited.insert(next); queue.enqueue(next);
                }
            }
            const bool continuous = visited.contains(end);
            bool verticalContinuous = true;
            for (int y = 2 * scale; y <= 30 * scale; ++y)
                verticalContinuous &= ink(24 * scale, y);
            const bool oldHorizontalCleared = !ink(20 * scale, 16 * scale) && !ink(28 * scale, 16 * scale);
            QApplication::setPalette(originalPalette);
            require(continuous, QString("Bridge has a visible gap at %1x in %2 theme")
                        .arg(scale).arg(dark ? "dark" : "light"));
            require(verticalContinuous, QString("Bridge mask cuts the vertical line at %1x in %2 theme")
                        .arg(scale).arg(dark ? "dark" : "light"));
            require(oldHorizontalCleared, "Original horizontal stroke remains underneath the bridge");
            QApplication::setPalette(palette);
        }
    }
    QApplication::setPalette(originalPalette);
}

void checkSiblingBranches(const QString& directory) {
    FlowsheetScene scene;
    auto* a = new FlotationUnitItem({"a", {0, 200}, 100});
    auto* b = new FlotationUnitItem({"b", {-300, 300}, 100});
    auto* mill = new FlotationUnitItem({"mill", {200, -200}, 180, 100, UnitKind::BallMill});
    scene.addItem(a); scene.addItem(b); scene.addItem(mill);
    auto* first = a->products().front(); auto* second = b->products().front();
    auto* feed = scene.connectRecycle(first, mill->inputLine());
    require(feed && scene.connectRecycle(second, mill->inputLine()) == feed,
            "Sibling inlet branches must belong to one feed item");
    feed->setManualRouteX(first->streamId(), -700); feed->setManualRouteY(first->streamId(), -300);
    feed->setManualRouteX(second->streamId(), -600); feed->setManualRouteY(second->streamId(), -330);
    const QPointF crossing(-350, 350);
    const auto before = renderBridges(scene);
    require(contains(before, crossing), "Crossing branches inside one feed item have no bridge");
    require(contains(before, {-600, 350}), "Sibling branch corridor has no bridge");
    require(!contains(before, {-700, 350}) && !contains(before, feed->junctionPosition()),
            "A route bend or explicit inlet junction was drawn as a bridge");
    checkContinuity(scene, crossing, directory);
    const QPointF delta(80, -40);
    a->setPos(a->pos() + delta);
    require(contains(renderBridges(scene), crossing + delta), "Bridge did not follow group movement");
    require(scene.disconnectFeedSource(feed, second->streamId()), "Could not detach inlet branch");
    // Detaching restores the source's terminal arrow, so its vertical segment
    // can still cross the first branch. Only the removed corridor disappears.
    require(!contains(renderBridges(scene), QPointF(-600, 350) + delta),
            "Removed inlet corridor left a stale bridge");
}

void checkProject(const QString& directory) {
    QDir root(QCoreApplication::applicationDirPath()); root.cdUp(); root.cdUp();
    FlowsheetScene scene; FlowsheetDocument document; QString error;
    require(ProjectSerializer::load(scene, document, root.filePath("tests/test.json"), &error), error);
    FeedJunctionItem* feed = nullptr;
    for (auto* item : scene.items())
        if (auto* candidate = dynamic_cast<FeedJunctionItem*>(item);
            candidate && candidate->targetUnit()->unit().kind == UnitKind::BallMill) feed = candidate;
    require(feed && feed->recycleProducts().size() == 5, "Unexpected ball-mill inlet fixture");
    // The fifth branch passes across the first three corridors; the fourth
    // passes across those same corridors and the fifth. The third branch's
    // lower horizontal segment also crosses the first two corridors. All nine
    // are between branches of the same feed item, not topology junctions.
    const auto& xs = feed->manualRouteXs();
    const auto junction = feed->junctionPosition();
    const auto branch = feed->recycleProducts()[2];
    QVector<QPointF> expected;
    for (const auto& id : {"66:left", "78:left", "90:left"}) {
        expected.append({xs.value(id), junction.y() - 4 * 18});
        expected.append({xs.value(id), junction.y() - 5 * 18});
    }
    expected.append({xs.value("29:right"), junction.y() - 4 * 18});
    for (const auto& id : {"66:left", "78:left"})
        expected.append({xs.value(id), branch->unconnectedEndScenePosition().y()});
    const auto saved = ProjectSerializer::serialize(scene, document);
    const auto centers = renderBridges(scene);
    for (const auto& crossing : expected)
        require(contains(centers, crossing), QString("Fixture inlet crossing lacks bridge at %1,%2")
                    .arg(crossing.x()).arg(crossing.y()));
    require(!contains(centers, junction), "Ball-mill inlet junction must remain connected");
    require(ProjectSerializer::serialize(scene, document) == saved, "Bridge rendering changed project topology");
    const auto path = directory + "/ball-mill-inlet-bridges.svg";
    require(SvgExporter::exportScene(scene, path, "inlet bridges"), "Could not export bridge fixture");
    QFile svg(path); require(svg.open(QIODevice::ReadOnly), "Cannot read bridge export");
    // The scene foreground must also run during ordinary scene export.
    const auto exported = bridgeCenters(svg.readAll());
    for (const auto& crossing : expected)
        require(contains(exported, crossing), "Scene export omitted a ball-mill inlet bridge");
    qInfo() << "Nine crossings between ball-mill inlet branches retain bridges in project and SVG export";
}
}

void runCrossingBridgeChecks(const QString& directory) {
    checkSiblingBranches(directory); checkProject(directory);
}

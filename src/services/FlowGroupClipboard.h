#pragma once

#include "core/FlotationUnit.h"

#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>
#include <functional>
#include <optional>

namespace afs {
class FlowsheetDocument;
class FlowsheetScene;

// An in-process flowsheet fragment.  It deliberately stores only topology
// wholly owned by the copied units, so paste can never reconnect a clone to
// an unrelated part of the source diagram.
class FlowGroupClipboard final {
public:
    struct CopyResult { int units{0}; int connections{0}; int merges{0}; int feeds{0}; };
    struct PasteResult { int units{0}; int connections{0}; int merges{0}; int feeds{0}; };

    [[nodiscard]] bool hasData() const { return !m_units.isEmpty(); }
    [[nodiscard]] CopyResult copy(const FlowsheetScene& scene, const FlowsheetDocument& document);
    [[nodiscard]] PasteResult paste(FlowsheetScene& scene, FlowsheetDocument& document,
                                    const std::function<QString()>& nextUnitId);

private:
    struct Direct { QString source; QString target; std::optional<double> routeY; };
    struct Merge { QString id; QVector<QString> sources; QString target; std::optional<double> y; };
    struct Feed {
        QString target; QString processType; QString processId;
        bool hasExternalFeed{false};
        QVector<QString> recycleTypes; QVector<QString> recycleIds;
        QHash<QString, double> routeXs; QHash<QString, double> routeYs;
    };
    QVector<FlotationUnit> m_units;
    QVector<Direct> m_direct;
    QVector<Merge> m_merges;
    QVector<Feed> m_feeds;
    QHash<QString, QString> m_productNames;
    int m_pasteCount{0};
};
} // namespace afs

#pragma once

#include "core/FlotationUnit.h"
#include "annotations/AnnotationTypes.h"
#include <QHash>
#include <QStringList>
#include <QVector>

namespace afs::flow_group {

struct CopyResult { int units{0}; int connections{0}; int merges{0}; int feeds{0}; int notes{0}; int reagents{0}; };
struct PasteResult {
    int units{0}; int connections{0}; int merges{0}; int feeds{0};
    QStringList noteIds;
    int reagents{0};
};

// Value snapshot only: capture and paste never retain pointers into the
// original scene. Coordinates keep the existing local/scene conventions.
struct Snapshot {
    struct Direct { QString source; QString target; std::optional<double> routeY; };
    struct Merge { QString id; QVector<QString> sources; QString target; std::optional<double> y; };
    struct Feed {
        QString id;
        QString target; QString processType; QString processId;
        bool hasExternalFeed{false};
        QVector<QString> recycleTypes; QVector<QString> recycleIds;
        QHash<QString, double> routeXs; QHash<QString, double> routeYs;
    };
    QVector<FlotationUnit> units;
    QVector<Direct> direct;
    QVector<Merge> merges;
    QVector<Feed> feeds;
    QHash<QString, QString> productNames;
    QHash<QString, double> terminalLengths;
    QVector<AnnotationRecord> notes;
    QVector<AnnotationRecord> reagents;

    bool hasData() const { return !units.isEmpty() || !notes.isEmpty(); }
};

} // namespace afs::flow_group

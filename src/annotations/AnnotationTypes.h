#pragma once

#include <QPointF>
#include <QColor>
#include <QString>

namespace afs {

enum class AnnotationKind { StreamResult, Reagent, FlotationTime, UserNote };
enum class AnnotationStyle { ResultCard, PlainText, Note, Warning };
enum class AnnotationOwnerKind { Stream, FlotationUnit, Free };
enum class ResultMetric { DryMass, Grade, OverallYield, OverallRecovery };
enum class MetricLabelMode { Chinese, Symbols };

struct AnnotationTextSettings {
    int pointSize{9};
    bool bold{false};
    QColor color;
};

struct ResultAnnotationSettings {
    bool showDryMass{true};
    bool showGrade{true};
    bool showOverallYield{false};
    bool showOverallRecovery{false};
    int decimalPlaces{2};
    MetricLabelMode labelMode{MetricLabelMode::Chinese};

    [[nodiscard]] bool visible(ResultMetric metric) const {
        switch (metric) {
        case ResultMetric::DryMass: return showDryMass;
        case ResultMetric::Grade: return showGrade;
        case ResultMetric::OverallYield: return showOverallYield;
        case ResultMetric::OverallRecovery: return showOverallRecovery;
        }
        return false;
    }
    [[nodiscard]] bool anyVisible() const {
        return showDryMass || showGrade || showOverallYield || showOverallRecovery;
    }
};

struct AnnotationRecord {
    QString id;
    AnnotationKind kind{AnnotationKind::StreamResult};
    AnnotationStyle style{AnnotationStyle::ResultCard};
    AnnotationOwnerKind ownerKind{AnnotationOwnerKind::Stream};
    QString ownerId;
    QPointF manualOffset;
    bool manuallyPlaced{false};
    bool visible{true};
    QString text;
    QString dosage;
    QString dosageUnit;
    QString note;
    QString noteFontFamily;
    int notePointSize{0};
    bool noteBold{false};
    bool noteBorderVisible{true};
};

} // namespace afs

#pragma once

#include <QPointF>
#include <QColor>
#include <QColor>
#include <QString>
#include <algorithm>
#include <cmath>

namespace afs {

enum class AnnotationKind { StreamResult, Reagent, FlotationTime, UserNote };
enum class AnnotationStyle { ResultCard, PlainText, Note, Warning };
enum class AnnotationOwnerKind { Stream, FlotationUnit, Free };
enum class ResultMetric { DryMass, Grade, OverallYield, OverallRecovery };
enum class MetricLabelMode { Chinese, Symbols };
enum class MassUnit { Gram, Kilogram, Tonne, Custom };

struct ResultBorderStyle {
    static constexpr qreal DefaultWidth = 2.4;
    static constexpr qreal MinimumWidth = 1.2;
    static constexpr qreal MaximumWidth = 8.0;

    [[nodiscard]] static qreal boundedWidth(qreal width) {
        return std::isfinite(width) ? std::clamp(width, MinimumWidth, MaximumWidth) : DefaultWidth;
    }
};

[[nodiscard]] inline QString massUnitSymbol(MassUnit unit) {
    switch (unit) {
    case MassUnit::Gram: return QStringLiteral("g");
    case MassUnit::Kilogram: return QStringLiteral("kg");
    case MassUnit::Tonne: return QStringLiteral("t");
    case MassUnit::Custom: return {};
    }
    return QStringLiteral("g");
}

struct AnnotationTextSettings {
    int pointSize{9};
    bool bold{false};
    QColor color;
    MassUnit massUnit{MassUnit::Gram};
    QString customMassUnit;
    bool resultAnnotationsVisible{true};
    bool showDryMass{true};
    bool showGrade{true};
    bool showOverallYield{false};
    bool showOverallRecovery{false};
    // Product names are an optional heading for result cards.  Keep this
    // separate from metrics because it is descriptive rather than calculated.
    bool showProductName{false};
    MetricLabelMode metricLabelMode{MetricLabelMode::Chinese};
};

struct ResultAnnotationSettings {
    bool showDryMass{true};
    bool showGrade{true};
    bool showOverallYield{false};
    bool showOverallRecovery{false};
    bool showProductName{false};
    int decimalPlaces{2};
    MetricLabelMode labelMode{MetricLabelMode::Chinese};
    MassUnit massUnit{MassUnit::Gram};
    QString customMassUnit;

    bool distinguishValueSources{true};

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
        return showDryMass || showGrade || showOverallYield || showOverallRecovery
            || showProductName;
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
    // Invalid means follow AnnotationTextSettings::color/theme text color.
    QColor noteColor;
};

} // namespace afs

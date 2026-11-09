#pragma once
#include <QString>
#include <QVector>

namespace afs {
class FlowsheetScene;
struct NumericProperty {
    QString key, label;
    double value{0}, minimum{0}, maximum{5000};
};
struct PropertyBranch { QString id, label; };
struct ObjectPropertyState {
    QString objectKey, title, streamId;
    QVector<NumericProperty> fields;
    QVector<PropertyBranch> branches;
    QString activeBranch;
    bool canResetRoute{false};
};
class CanvasProperties final {
public:
    static ObjectPropertyState describe(const FlowsheetScene& scene);
    static bool apply(FlowsheetScene& scene, const QString& objectKey, const QString& key, double value);
    static bool resetRoutes(FlowsheetScene& scene, const QString& objectKey);
    static bool selectBranch(FlowsheetScene& scene, const QString& objectKey, const QString& branch);
};
}

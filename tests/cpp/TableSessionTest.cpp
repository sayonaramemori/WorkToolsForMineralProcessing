#include "app/TerminalTableController.h"
#include "document/FlowsheetDocument.h"
#include <QCoreApplication>
#include <QDebug>

using namespace afs;
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    FlowsheetDocument document;
    TerminalTableController session(document);
    session.setStreams({{"a", "物流 A"}, {"locked", "只读测量"}}, QSet<QString>{"a"});
    const auto initial = session.snapshot();
    const auto measurement = [&](QString text, MeasurementField field = MeasurementField::DryMass,
                                 QString component = {}) {
        return session.submit(EditTableMeasurement{"a", component, field, text});
    };
    if (!measurement("25.5") || !measurement("3.2", MeasurementField::Grade, "component-1")
        || document.measurement("a").dryMass != 25.5 || session.snapshot().measurements.value("a").grade("component-1") != 3.2
        || !initial.measurements.isEmpty()) return 1;
    for (const auto& invalid : {"-1", "not a number", "inf", "nan"})
        if (measurement(invalid)) return 2;
    if (measurement("101", MeasurementField::Grade, "component-1")
        || measurement("5", MeasurementField::Grade, "unknown")
        || measurement("1", static_cast<MeasurementField>(99))
        || measurement("1", MeasurementField::MassStdDev)) return 3;
    if (session.submit(EditTableMeasurement{"missing", {}, MeasurementField::DryMass, "10"})
        || session.submit(EditTableMeasurement{"locked", {}, MeasurementField::DryMass, "10"})) return 4;
    QString changedId;
    TableChange change = TableChange::All;
    QObject::connect(&session, &TerminalTableSession::stateChanged,
        [&](const TerminalTableState&, const QString& id, TableChange kind) { changedId = id; change = kind; });
    if (!session.submit(EditTableProductName{"locked", "  精矿  "})
        || document.productName("locked") != "精矿" || changedId != "locked" || change != TableChange::ProductName)
        return 5;
    if (!session.submit(EditTableMode{CalculationMode::DataReconciliation})
        || !measurement("0.5", MeasurementField::MassStdDev)
        || measurement("0", MeasurementField::MassStdDev)
        || !measurement("", MeasurementField::MassStdDev)
        || document.measurement("a").dryMassStdDev) return 6;
    if (session.submit(EditTableMode{static_cast<CalculationMode>(99)})) return 7;
    if (!session.submit(EditTableComponents{{" Cu ", "", "Zn", "Zn"}})
        || document.components().size() != 2 || document.components()[0].id != "component-1"
        || document.components()[0].name != "Cu" || document.components()[1].name != "Zn") return 8;
    const auto schema = session.snapshot();
    if (!measurement("2", MeasurementField::Grade, "component-2")
        || !measurement("0.1", MeasurementField::GradeStdDev, "component-2")
        || schema.measurements.value("a").grade("component-2")) return 9;
    topology::CalculationResult calculated;
    calculated.values.insert("a", {25.5, 1.0});
    document.setCalculationResult(calculated);
    if (!session.snapshot().calculation || !measurement("30") || session.snapshot().calculation) return 10;
    const int empty = document.addScenario("空白", false);
    document.setCurrentScenario(empty);
    if (!session.snapshot().measurements.isEmpty()) return 11;
    document.setCurrentScenario(0);
    if (session.snapshot().measurements.value("a").dryMass != 30) return 12;
    session.setStreams({{"replacement", "新项目物流"}});
    if (measurement("99") || session.submit(EditTableProductName{"a", "旧编辑器提交"})
        || document.measurement("a").dryMass != 30) return 13;
    if (!session.submit(EditTableMeasurement{"replacement", {}, MeasurementField::DryMass, "10"})) return 14;
    qInfo() << "Application table session validation checks passed";
    return 0;
}

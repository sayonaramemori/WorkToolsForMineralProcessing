#pragma once

#include "document/StreamMeasurement.h"
#include "document/ExperimentScenario.h"
#include "document/ComponentDefinition.h"
#include "topology/TopologyTypes.h"
#include "annotations/AnnotationTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

namespace afs {

class FlowsheetDocument final : public QObject {
    Q_OBJECT
public:
    explicit FlowsheetDocument(QObject* parent = nullptr);

    [[nodiscard]] StreamMeasurement measurement(const QString& streamId) const;
    [[nodiscard]] const QHash<QString, StreamMeasurement>& measurements() const;
    [[nodiscard]] QString productName(const QString& streamId) const;
    [[nodiscard]] const QHash<QString, QString>& productNames() const { return m_productNames; }
    [[nodiscard]] const QStringList& exportStreamOrder() const { return m_exportStreamOrder; }
    void setExportStreamOrder(QStringList streamIds);
    void setProductName(const QString& streamId, QString name);
    [[nodiscard]] const AnnotationTextSettings& annotationTextSettings() const {
        return m_annotationTextSettings;
    }
    void setAnnotationTextSettings(AnnotationTextSettings settings);
    void setDryMass(const QString& streamId, std::optional<double> value);
    void setDryMassStdDev(const QString& streamId, std::optional<double> value);
    void setGradePercent(const QString& streamId, std::optional<double> value);
    void setGradePercent(const QString& streamId, const QString& componentId,
                         std::optional<double> value);
    void setGradeStdDev(const QString& streamId, const QString& componentId,
                        std::optional<double> value);
    [[nodiscard]] CalculationMode calculationMode() const;
    void setCalculationMode(CalculationMode mode);
    void setComponentSharePercent(const QString& streamId, const QString& componentId,
                                  std::optional<double> value);
    void setDryMassSharePercent(const QString& streamId, std::optional<double> value);
    void setComponentSharePercent(const QString& streamId, std::optional<double> value);
    [[nodiscard]] const QVector<ComponentDefinition>& components() const { return m_components; }
    [[nodiscard]] QStringList componentIds() const;
    void setComponents(QVector<ComponentDefinition> components);
    [[nodiscard]] const topology::CalculationResult* calculationResult() const;
    void setCalculationResult(topology::CalculationResult result);
    void invalidateCalculation();
    void invalidateAllCalculations();
    [[nodiscard]] AnnotationRecord annotationRecord(const QString& annotationId) const;
    [[nodiscard]] QHash<QString, AnnotationRecord> annotationRecords() const;
    void setAnnotationRecord(AnnotationRecord record);
    void removeAnnotationRecord(const QString& annotationId);
    void replaceProjectData(QHash<QString, StreamMeasurement> measurements,
                            QHash<QString, AnnotationRecord> annotations,
                            QHash<QString, QString> productNames = {},
                            AnnotationTextSettings annotationTextSettings = {},
                            QStringList exportStreamOrder = {});
    [[nodiscard]] const QVector<ExperimentScenario>& scenarios() const { return m_scenarios; }
    [[nodiscard]] int currentScenarioIndex() const { return m_currentScenarioIndex; }
    [[nodiscard]] QString currentScenarioName() const;
    bool setCurrentScenario(int index);
    int addScenario(QString name, bool copyCurrent);
    bool renameScenario(int index, QString name);
    bool removeScenario(int index);
    void replaceScenarios(QVector<ExperimentScenario> scenarios, int currentIndex);
    void removeUnknownStreams(const QSet<QString>& validStreamIds);

signals:
    void projectChanged();
    void measurementChanged(const QString& streamId);
    void productNameChanged(const QString& streamId);
    void annotationTextSettingsChanged();
    void componentsChanged();
    void calculationChanged();
    void scenariosChanged();
    void currentScenarioChanged();
    void calculationModeChanged();

private:
    QHash<QString, QString> m_productNames;
    QStringList m_exportStreamOrder;
    QHash<QString, AnnotationRecord> m_sharedAnnotations;
    AnnotationTextSettings m_annotationTextSettings;
    QVector<ComponentDefinition> m_components{{DefaultComponentId, DefaultComponentName}};
    QVector<ExperimentScenario> m_scenarios;
    int m_currentScenarioIndex{0};
    int m_nextScenarioId{2};
    ExperimentScenario& currentScenario();
    const ExperimentScenario& currentScenario() const;
};

} // namespace afs

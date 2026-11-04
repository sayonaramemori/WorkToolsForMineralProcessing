#include "document/FlowsheetDocument.h"
#include <QSet>
#include <algorithm>
#include <utility>

namespace afs {

ExperimentScenario& FlowsheetDocument::currentScenario() { return m_scenarios[m_currentScenarioIndex]; }

const ExperimentScenario& FlowsheetDocument::currentScenario() const { return m_scenarios[m_currentScenarioIndex]; }

QString FlowsheetDocument::currentScenarioName() const { return currentScenario().name; }

bool FlowsheetDocument::setCurrentScenario(int index) {
    if (index < 0 || index >= m_scenarios.size() || index == m_currentScenarioIndex) return false;
    m_currentScenarioIndex = index;
    emit projectChanged();
    emit currentScenarioChanged(); emit calculationChanged(); emit measurementChanged({});
    emit calculationModeChanged();
    return true;
}

int FlowsheetDocument::addScenario(QString name, bool copyCurrent) {
    name = name.simplified().left(80);
    if (name.isEmpty()) return -1;
    ExperimentScenario scenario;
    if (copyCurrent) scenario = currentScenario();
    scenario.id = QString("scenario-%1").arg(m_nextScenarioId++);
    scenario.name = std::move(name);
    m_scenarios.append(std::move(scenario));
    emit projectChanged();
    emit scenariosChanged();
    return m_scenarios.size() - 1;
}

bool FlowsheetDocument::renameScenario(int index, QString name) {
    name = name.simplified().left(80);
    if (index < 0 || index >= m_scenarios.size() || name.isEmpty()) return false;
    m_scenarios[index].name = std::move(name); emit projectChanged(); emit scenariosChanged(); return true;
}

bool FlowsheetDocument::removeScenario(int index) {
    if (m_scenarios.size() <= 1 || index < 0 || index >= m_scenarios.size()) return false;
    if (index < m_currentScenarioIndex) --m_currentScenarioIndex;
    m_scenarios.removeAt(index);
    m_currentScenarioIndex = std::min(m_currentScenarioIndex,
                                      static_cast<int>(m_scenarios.size()) - 1);
    emit projectChanged();
    emit scenariosChanged(); emit currentScenarioChanged(); emit calculationChanged(); emit measurementChanged({});
    emit calculationModeChanged();
    return true;
}

void FlowsheetDocument::replaceScenarios(QVector<ExperimentScenario> scenarios, int currentIndex) {
    if (scenarios.isEmpty()) return;
    m_scenarios = std::move(scenarios);
    m_currentScenarioIndex = std::clamp(currentIndex, 0,
                                        static_cast<int>(m_scenarios.size()) - 1);
    int maximum = 0;
    for (const auto& item : m_scenarios) {
        bool ok = false; const int value = item.id.mid(QString("scenario-").size()).toInt(&ok);
        if (ok) maximum = std::max(maximum, value);
    }
    m_nextScenarioId = maximum + 1;
    emit scenariosChanged(); emit currentScenarioChanged(); emit calculationChanged(); emit measurementChanged({});
    emit calculationModeChanged();
}

} // namespace afs

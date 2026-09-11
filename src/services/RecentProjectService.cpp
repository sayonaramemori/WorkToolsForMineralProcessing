#include "services/RecentProjectService.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace afs {
namespace {
constexpr auto RecentProjectsKey = "projects/recentFiles";
constexpr int MaximumRecentProjects = 10;
}

QString RecentProjectService::normalizedPath(const QString& path) {
    if (path.trimmed().isEmpty()) return {};
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

QStringList RecentProjectService::projects() {
    QStringList result;
    const auto stored = QSettings().value(RecentProjectsKey).toStringList();
    for (const auto& path : stored) {
        const QString normalized = normalizedPath(path);
        if (!normalized.isEmpty() && !result.contains(normalized, Qt::CaseSensitive))
            result.append(normalized);
        if (result.size() == MaximumRecentProjects) break;
    }
    return result;
}

void RecentProjectService::addProject(const QString& path) {
    const QString normalized = normalizedPath(path);
    if (normalized.isEmpty()) return;
    auto recent = projects();
    recent.removeAll(normalized);
    recent.prepend(normalized);
    while (recent.size() > MaximumRecentProjects) recent.removeLast();
    QSettings().setValue(RecentProjectsKey, recent);
}

void RecentProjectService::removeProject(const QString& path) {
    auto recent = projects();
    recent.removeAll(normalizedPath(path));
    QSettings().setValue(RecentProjectsKey, recent);
}

void RecentProjectService::clear() {
    QSettings().remove(RecentProjectsKey);
}

} // namespace afs

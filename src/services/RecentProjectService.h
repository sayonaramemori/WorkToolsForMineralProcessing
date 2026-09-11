#pragma once

#include <QString>
#include <QStringList>

namespace afs {

class RecentProjectService final {
public:
    RecentProjectService() = delete;

    static QStringList projects();
    static void addProject(const QString& path);
    static void removeProject(const QString& path);
    static void clear();

private:
    static QString normalizedPath(const QString& path);
};

} // namespace afs
